#include "utils.h"
#include "lib/printk.h"
#include "lib/heap.h"
#include "display/video_fb.h"

#include "hwinit/btn.h"
#include "hwinit/hwinit.h"
#include "hwinit/di.h"
#include "hwinit/mc.h"
#include "hwinit/t210.h"
#include "hwinit/sdmmc.h"
#include "hwinit/timer.h"
#include "hwinit/cluster.h"
#include "hwinit/clock.h"
#include "hwinit/util.h"
#include "rcm_usb.h"
#include "storage.h"
#include "lib/ff.h"
#include "lib/decomp.h"
#include "iniparse.h"
#include "cbmem.h"
#include <alloca.h>
#include <strings.h>
#define XVERSION 1

static int initialize_mount(FATFS* outFS, u8 devNum)
{
	sdmmc_t* currCont = get_controller_for_index(devNum);
    sdmmc_storage_t* currStor = get_storage_for_index(devNum);

    if (currCont == NULL || currStor == NULL)
        return 0;

    if (currStor->sdmmc != NULL)
        return 1; //already initialized

    if (devNum == 0) //maybe support more ?
    {
        if (sdmmc_storage_init_sd(currStor, currCont, SDMMC_1, SDMMC_BUS_WIDTH_4, 11) && f_mount(outFS, "", 1) == FR_OK)
            return 1;
        else
        {
            if (currStor->sdmmc != NULL)
                sdmmc_storage_end(currStor, 0);

            memset(currCont, 0, sizeof(sdmmc_t));
            memset(currStor, 0, sizeof(sdmmc_storage_t));
        }
    }

	return 0;
}

static void deinitialize_storage()
{
    f_unmount("");
    for (u32 i=0; i<FF_VOLUMES; i++)
    {
        sdmmc_storage_t* stor = get_storage_for_index((u8)i);
        if (stor != NULL && stor->sdmmc != NULL)
        {
            if (!sdmmc_storage_end(stor, 1))
                dbg_print("sdmmc_storage_end for storage idx %u FAILED!\n", i);
            else
                memset(stor, 0, sizeof(sdmmc_storage_t));
        }
    }
}

static NOINLINE int display_file_picker(char* outFilenameBuf, size_t* outFilesizeBuf, int currSelection)
{
    typedef struct fileEntry_s
    {
        char* fileName;
        size_t fileSize;
        struct fileEntry_s* next;
    } fileEntry_t;

    fileEntry_t* files = NULL;
    fileEntry_t* lastFile = NULL;
    u32 numFiles = 0;

    DIR dir;
    memset(&dir, 0, sizeof(dir));

    FRESULT res = f_opendir(&dir, "/");
    if (res == FR_OK) 
    {
        while (res == FR_OK)
        {
            FILINFO fno;
            memset(&fno, 0, sizeof(fno));

            res = f_readdir(&dir, &fno);
            if (res != FR_OK)
                break;
            if (fno.fname[0] == 0 && fno.altname[0] == 0)
                break;

            if (fno.fattrib & AM_DIR)
                continue;
            else 
            {
                const char* nameStr = NULL;
                size_t nameLen = 0;
                if (fno.fname[0] != 0)
                {
                    nameStr = fno.fname;
                    nameLen = strlen(fno.fname);
                }
                else
                {
                    nameStr = fno.altname;
                    nameLen = strlen(fno.altname);
                }

                static const char REQUIRED_EXTENSION[] = ".ini";
                if (nameLen < ARRAY_SIZE(REQUIRED_EXTENSION)-1)
                    continue;

                if (strcasecmp(&nameStr[nameLen-(ARRAY_SIZE(REQUIRED_EXTENSION)-1)], REQUIRED_EXTENSION))
                    continue;

                if (lastFile == NULL)
                {
                    files = alloca(sizeof(fileEntry_t));
                    lastFile = files;           
                }
                else
                {
                    lastFile->next = alloca(sizeof(fileEntry_t));
                    lastFile = lastFile->next;
                }

                lastFile->fileName = alloca(nameLen+1);
                memcpy(lastFile->fileName, nameStr, nameLen+1);

                lastFile->fileSize = fno.fsize;
                lastFile->next = NULL;
                numFiles++;
            }
        }
        f_closedir(&dir);
        memset(&dir,0,sizeof(dir));
    }

    if (files == NULL)
        printk("No ini files found in root of sdcard, switching to USB command mode...\n");
    else
    {
        printk("Choose ini file from sdcard root:\n\n");
        int startRow = video_get_row();
        int startCol = video_get_col();

        u32 lastTime = get_tmr();
        u32 currTime = lastTime;
        u32 btnReadTime = currTime;
        if (currSelection < 0)
        {
            currSelection = 0;
            lastFile = files;
            while (lastFile != NULL)
            {
                static const char PREFERRED_FILENAME[] = "memloader.ini";
                if (!strcasecmp(lastFile->fileName, PREFERRED_FILENAME))
                    break;

                currSelection++;
                lastFile = lastFile->next;
            }
        }        

        for (;;)
        {
            bool up_pressed = false;
            bool down_pressed = false;
            bool pwr_pressed = false;
            if (currTime - btnReadTime > 100000) //10/s key repeat rate
            {
                u32 btns = btn_read();
                up_pressed = (btns & BTN_VOL_UP) != 0;
                down_pressed = (btns & BTN_VOL_DOWN) != 0;
                pwr_pressed = (btns & BTN_POWER) != 0;

                btnReadTime = currTime;
            }

            if (down_pressed)
                currSelection = (currSelection+1) % (numFiles+1);
            if (up_pressed)
                currSelection = (currSelection == 0) ? (numFiles) : (currSelection-1);

            fileEntry_t* selectedFile = NULL;
            video_reposition(startRow, startCol);
            lastFile = files;
            int currFile = 0;
            for (;;)
            {
                if (lastFile == NULL)
                    video_puts("\n");

                if (currSelection == currFile)
                {
                    selectedFile = lastFile;
                    video_puts("   >");
                }
                else
                    video_puts("    ");

                video_puts("  ");
                if (lastFile == NULL)
                {
                    video_puts("--- Go to USB command mode instead ---");
                    video_clear_line();
                    break;
                }
                else
                {
                    video_puts(lastFile->fileName);
                    //printk(" (%u bytes)", lastFile->fileSize);
                    video_clear_line();
                }

                currFile++;
                lastFile = lastFile->next;
            }

            currTime = get_tmr();
            if (pwr_pressed)
            {
                if (selectedFile == NULL)
                    return 0;
                else
                {
                    memcpy(outFilenameBuf, selectedFile->fileName, strlen(selectedFile->fileName)+1);
                    *outFilesizeBuf = selectedFile->fileSize;
                    return currSelection+1;
                }
            }
        }
    }  

    return 0; 
}

static NOINLINE int read_and_parse_ini(const char* pickedName, size_t pickedSize, IniParsedInfo_t* outInfoPtr)
{    
    FIL fp;
    memset(&fp, 0, sizeof(fp));

    FRESULT res = f_open(&fp, pickedName, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK)
    {
        printk("Error %d opening file '%s' from sd card, try another?", pickedName);
        video_clear_line();
        return 0;
    }
    else
    {
        char* iniBytes = alloca(pickedSize+1);
        UINT bytesRead = 0;
        res = f_read(&fp, iniBytes, pickedSize, &bytesRead);
        f_close(&fp);

        if (res != FR_OK)
        {   
            printk("Error %d reading %u bytes from file '%s' on sd card, try another?", pickedSize, pickedName);
            video_clear_line();
            return 0;
        }
        else if (bytesRead != pickedSize)
            printk("Warning, only read %u out of %u bytes from file '%s' on sd card\n", bytesRead, pickedSize, pickedName);

        iniBytes[bytesRead] = 0;
        printk("Read %u bytes from '%s', parsing...", bytesRead, pickedName);
        video_clear_line();

        *outInfoPtr = parse_memloader_ini(iniBytes, bytesRead, malloc, (ErrPrintFunc)printk);
        return 1;
    }
}

static NOINLINE int execute_copy_section(IniCopySection_t* sect)
{
    int retVal = 0;

    const char* opTypeName = "UNKNOWN";
    if (sect->compType == 0)
        opTypeName = "COPY";
    else if (sect->compType == 1)
        opTypeName = "UNLZMA";
    else if (sect->compType == 2)
        opTypeName = "UNLZ4";

    printk("%s '%s' [0x%08x,0x%08x] -> [0x%08x,0x%08x]...", opTypeName, sect->sectname, 
            sect->src, sect->srclen, sect->dst, sect->dstlen);

    if (sect->compType == 0)
    {
        if (sect->srclen > 0)
            memcpy((void*)sect->dst, (void*)sect->src, sect->srclen);

        if (sect->dstlen > sect->srclen)
            memset((u8*)sect->dst+sect->srclen, 0, sect->dstlen-sect->srclen);

        printk("OK!");
        retVal = (int)sect->srclen;
    }
    else
    {
        size_t len = 0;
        memset((void*)sect->dst, 0, sect->dstlen);

        if (sect->compType == 1)
            len = ulzman((void*)sect->src, sect->srclen, (void*)sect->dst, sect->dstlen);
        else if (sect->compType == 2)
            len = ulz4fn((void*)sect->src, sect->srclen, (void*)sect->dst, sect->dstlen);

        if (len == 0)
            printk("ERROR!");
        else
            printk("OK! (%u bytes)", len);

        retVal = (int)len;
    }
    video_clear_line();
    return retVal;
}

static NOINLINE int execute_boot_section(IniBootSection_t* sect)
{
    printk("BOOT section '%s'\n\tpc=0x%08x", sect->sectname, sect->pc);
    video_clear_line();

    if (sect->pc == 0)
        return 0;

    deinitialize_storage();
    sleep(1000);
    mc_disable_ahb_redirect();

    cluster_boot_cpu0(sect->pc);
    clock_halt_bpmp();

    return 1;
}

int main(void) 
{
    u32* lfb_base;    

    config_hw();
    display_enable_backlight(false);
    display_init();

    // Set up the display, and register it as a printk provider.
    lfb_base = display_init_framebuffer();
    video_init(lfb_base);

    //Tegra/Horizon configuration goes to 0x80000000+, package2 goes to 0xA9800000, we place our heap in between.
	heap_init(0x90020000);
    //Init the CBFS memory store in case we are booting coreboot
    cbmem_initialize_empty();

    mc_enable_ahb_redirect();

    printk("                                  memloader v%d by rajkosto\n", XVERSION);
    printk("\n atmosphere base by team reswitched, hwinit by naehrwert, some parts taken from coreboot\n\n");

    /* Turn on the backlight after initializing the lfb */
    /* to avoid flickering. */
    display_enable_backlight(true);

    FATFS fs;
    memset(&fs, 0, sizeof(FATFS));

    static const u32 DOTS_PER_LINE = 90;
    int pickerRow = video_get_row();
	if (!initialize_mount(&fs, 0))
    {
		printk("Failed to mount SD card, switching to USB command mode...\n");
        pickerRow++;
    }
    else
    {
        int lastDrawnRow = 0;
        int selectedFile = -1;
        for (;;)
        {
            char pickedName[FF_LFN_BUF + 1];
            size_t pickedSize = 0;

            video_reposition(pickerRow, 0);
            int myRes = display_file_picker(pickedName, &pickedSize, selectedFile);
            int postPickerRow = video_get_row();
            //clear out all the modified rows from last time
            if (lastDrawnRow != 0)
            {
                while (video_get_row() < lastDrawnRow) { video_puts(" "); video_clear_line(); }
                video_reposition(postPickerRow, 0);
                lastDrawnRow = 0;
            }
            if (myRes > 0)
            {
                selectedFile = myRes-1;

                IniParsedInfo_t infos;
                memset(&infos, 0, sizeof(IniParsedInfo_t));
                myRes = read_and_parse_ini(pickedName, pickedSize, &infos);
                if (myRes > 0)
                {
                    IniParsedInfo_t stackInfos;
                    memset(&stackInfos, 0, sizeof(IniParsedInfo_t));

                    #define STACK_STRDUP(srcString, dstPtr) \
                    if (srcString != NULL) {\
                        size_t nameLen = strlen(srcString); \
                        dstPtr = alloca(nameLen+1); \
                        memcpy(dstPtr, srcString, nameLen+1); } \
                    else { dstPtr = NULL; }

                    #define ALLOCA_NODE(currNodePtr, firstNodePtr, nodeType) \
                    if (currNodePtr == NULL) { \
                        firstNodePtr = alloca(sizeof(nodeType)); \
                        currNodePtr = firstNodePtr; } else { \
                        currNodePtr->next = alloca(sizeof(nodeType)); \
                        currNodePtr = currNodePtr->next; } \
                        currNodePtr->next = NULL

                    IniLoadSectionNode_t* currLoad = NULL;
                    for (IniLoadSectionNode_t* nod=infos.loads; nod!=NULL; nod=nod->next)
                    {
                        ALLOCA_NODE(currLoad, stackInfos.loads, IniLoadSectionNode_t);
                        memcpy(&currLoad->curr, &nod->curr, sizeof(nod->curr));
                        STACK_STRDUP(nod->curr.sectname, currLoad->curr.sectname);
                        STACK_STRDUP(nod->curr.filename, currLoad->curr.filename);
                    }
                    IniCopySectionNode_t* currCopy = NULL;
                    for (IniCopySectionNode_t* nod=infos.copies; nod!=NULL; nod=nod->next)
                    {
                        ALLOCA_NODE(currCopy, stackInfos.copies, IniCopySectionNode_t);
                        memcpy(&currCopy->curr, &nod->curr, sizeof(nod->curr));
                        STACK_STRDUP(nod->curr.sectname, currCopy->curr.sectname);
                    }
                    IniBootSectionNode_t* currBoot = NULL;
                    for (IniBootSectionNode_t* nod=infos.boots; nod!=NULL; nod=nod->next)
                    {
                        ALLOCA_NODE(currBoot, stackInfos.boots, IniBootSectionNode_t);
                        memcpy(&currBoot->curr, &nod->curr, sizeof(nod->curr));
                        STACK_STRDUP(nod->curr.sectname, currBoot->curr.sectname);
                    }
                    free_memloader_info(&infos, free);
                    memcpy(&infos, &stackInfos, sizeof(infos));

                    #undef STACK_STRDUP
                    #undef ALLOCA_NODE
                }

                bool operationFailed = false;

                for (IniLoadSectionNode_t* nod=infos.loads; nod!=NULL; nod=nod->next)
                {
                    if (operationFailed) break;

                    printk("LOAD '%s' (%s[0x%08x,0x%08x]) -> 0x%08x", nod->curr.sectname, nod->curr.filename, nod->curr.skip, nod->curr.count, nod->curr.dst);
                    video_clear_line();

                    size_t fileSize = 0;
                    {
                        FILINFO finfo;
                        memset(&finfo, 0, sizeof(finfo));
                        FRESULT res = f_stat(nod->curr.filename, &finfo);
                        if (res != FR_OK)
                        {
                            printk("ERROR %d inspecting file '%s', does it exist?", res, nod->curr.filename);
                            video_clear_line();
                            operationFailed = true;
                            break;
                        }
                        fileSize = (size_t)finfo.fsize;
                    }          

                    if (fileSize < nod->curr.skip)
                    {
                        printk("ERROR file '%s' is smaller than the start offset %u!", nod->curr.filename, nod->curr.skip);
                        operationFailed = true;
                        break;
                    }

                    size_t firstExtent = nod->curr.skip;
                    size_t lastExtent = fileSize;
                    size_t bytesToZero = 0;
                    if (nod->curr.count != 0)
                    {
                        lastExtent = nod->curr.skip + nod->curr.count;
                        if (lastExtent > fileSize)
                        {
                            bytesToZero = lastExtent - fileSize;
                            lastExtent = fileSize;
                        }
                    }

                    FIL fp;
                    memset(&fp, 0, sizeof(fp));
                    FRESULT res = f_open(&fp, nod->curr.filename, FA_READ | FA_OPEN_EXISTING);
                    if (res != FR_OK)
                    {
                        printk("ERROR %d opening file '%s'", res, nod->curr.filename);
                        video_clear_line();
                        operationFailed = true;
                        break;
                    }
                    else
                    {
                        res = f_lseek(&fp, firstExtent);
                        if (res != FR_OK)
                        {
                            printk("ERROR %d seeking to %u in file '%s'", res, firstExtent, nod->curr.filename);
                            video_clear_line();
                            operationFailed = true;
                            f_close(&fp);
                            break;
                        }

                        int numProgressDots = 0;
                        u32 progressDotBlockSize = (lastExtent-firstExtent)/DOTS_PER_LINE;
                        if (progressDotBlockSize < 1)
                            progressDotBlockSize = 1;
                        
                        u8* currMemAddr = (void*)nod->curr.dst;
                        for (size_t currFilePos = firstExtent; currFilePos<lastExtent;)
                        {
                            static const size_t READ_BLOCK_SIZE = 4*1024;
                            size_t bytesToRead = lastExtent-currFilePos;
                            if (bytesToRead > READ_BLOCK_SIZE)
                                bytesToRead = READ_BLOCK_SIZE;

                            UINT bytesRead = 0;
                            res = f_read(&fp, currMemAddr, bytesToRead, &bytesRead);
                            if (res != FR_OK)
                            {
                                printk("ERROR %d reading %u bytes from offset %u in file '%s'", res, bytesToRead, currFilePos, nod->curr.filename);
                                video_clear_line();
                                operationFailed = true;
                                break;
                            }

                            currFilePos += bytesRead;
                            currMemAddr += bytesRead;

                            int newProgressDots = (currFilePos-firstExtent)/progressDotBlockSize;
                            while (newProgressDots > numProgressDots)
                            {
                                video_puts(".");
                                numProgressDots++;
                            }
                        }
                        f_close(&fp);

                        if (!operationFailed && bytesToZero > 0)
                            memset(currMemAddr, 0, bytesToZero);

                        video_clear_line();
                    }
                }
                for (IniCopySectionNode_t* nod=infos.copies; nod!=NULL; nod=nod->next)
                {
                    if (operationFailed) break;

                    if (!execute_copy_section(&nod->curr))
                        operationFailed = true;
                }
                for (IniBootSectionNode_t* nod=infos.boots; nod!=NULL; nod=nod->next)
                {
                    if (operationFailed) break;
                    
                    if (!execute_boot_section(&nod->curr))
                        operationFailed = true;
                }
            }
            else
                break;

            lastDrawnRow = video_get_row();
            if (lastDrawnRow > postPickerRow)
                lastDrawnRow++;
            else
                lastDrawnRow = 0;
        }        

        deinitialize_storage();
    }

    //output to usb RCM host if it's listening
    if (!rcm_usb_device_ready()) //somehow usb isn't initialized
        printk("ERROR RCM USB not initialized, press the power button to turn off the system.\n");
    else
    {
        int lastDrawnRow = video_get_row();
        if (lastDrawnRow > pickerRow)
        {
            video_reposition(pickerRow, 0);
            while (video_get_row() <= lastDrawnRow) { video_puts(" "); video_clear_line(); }
            video_reposition(pickerRow, 0);
        }

        video_puts(" "); video_clear_line();
        video_reposition(video_get_row()-1, 0);

        static const char READY_NOTICE[] = "READY.\n";
        static const size_t USB_BLOCK_SIZE = 4*1024;
        u8 holdingBuffer[USB_BLOCK_SIZE*2];
        u8* usbBuffer = (void*)ALIGN_UP((u32)&holdingBuffer[0], USB_BLOCK_SIZE);
        int numTries = 0;
        for (;;)
        {
            int retVal = 0;
            unsigned int bytesTransferred = 0;
            while (retVal != 0 || bytesTransferred < ARRAY_SIZE(READY_NOTICE)-1)
            {
                printk("\rAttempting to communicate with USB host... try %d (last retVal %d xfer'd %u bytes)", ++numTries, retVal, bytesTransferred);
                memcpy(usbBuffer, READY_NOTICE, ARRAY_SIZE(READY_NOTICE));
                retVal = rcm_usb_device_write_ep1_in_sync(usbBuffer, ARRAY_SIZE(READY_NOTICE)-1, &bytesTransferred);
                if (retVal != 0)
                {
                    rcm_usb_device_reset_ep1();
                    if (btn_read() == BTN_POWER)
                        goto progend;
                }
            }

            enum
            {
                CMD_NONE,
                CMD_RECV,
                CMD_COPY,
                CMD_BOOT,
                CMD_COUNT
            } lastCommand = CMD_NONE;
            static const u32 BYTES_TO_RECV[CMD_COUNT] = { 4, 8, 20, 4 };
            
            video_clear_line();
            while (retVal == 0)
            {
                memset(usbBuffer, 0, 32);                
                bytesTransferred = 0;
                retVal = rcm_usb_device_read_ep1_out_sync(usbBuffer, BYTES_TO_RECV[lastCommand], &bytesTransferred);
                if (retVal != 0)
                {
                    printk("Error %d trying to read command from host (xfer'd %u bytes), going back to READY.", retVal, bytesTransferred);
                    video_clear_line(); video_puts(" "); video_clear_line();
                    break;
                }
                else if (bytesTransferred == 0) //short packet ?
                    continue;

                static const char RECV_COMMAND_STRING[] = "RECV";
                static const char COPY_COMMAND_STRING[] = "COPY";
                static const char BOOT_COMMAND_STRING[] = "BOOT";
                if (lastCommand == CMD_NONE)
                {
                    if (bytesTransferred == ARRAY_SIZE(RECV_COMMAND_STRING)-1 && !strcmp((char*)usbBuffer, RECV_COMMAND_STRING))
                        lastCommand = CMD_RECV;
                    else if (bytesTransferred == ARRAY_SIZE(COPY_COMMAND_STRING)-1 && !strcmp((char*)usbBuffer, COPY_COMMAND_STRING))
                        lastCommand = CMD_COPY;
                    else if (bytesTransferred == ARRAY_SIZE(BOOT_COMMAND_STRING)-1 && !strcmp((char*)usbBuffer, BOOT_COMMAND_STRING))
                        lastCommand = CMD_BOOT;
                    else
                    {
                        printk("Unknown command %s received with size %u bytes, retrying.\n", (char*)usbBuffer, bytesTransferred);
                        lastCommand = CMD_NONE;
                    }
                }                
                else if (lastCommand == CMD_RECV && bytesTransferred == BYTES_TO_RECV[CMD_RECV]) 
                {
                    u32 startAddr = __builtin_bswap32(*(u32*)(&usbBuffer[0]));
                    u32 xferLength = __builtin_bswap32(*(u32*)(&usbBuffer[4]));

                    printk("RECV 0x%08x bytes -> 0x%08x", xferLength, startAddr);
                    video_clear_line();

                    int numProgressDots = 0;
                    u32 progressDotBlockSize = xferLength/DOTS_PER_LINE;
                    if (progressDotBlockSize < 1)
                        progressDotBlockSize = 1;

                    u8* currMemAddr = (void*)startAddr;
                    u8* lastMemAddr = currMemAddr+xferLength;
                    while (currMemAddr < lastMemAddr)
                    {
                        size_t bytesToRecv = lastMemAddr-currMemAddr;
                        if (bytesToRecv > USB_BLOCK_SIZE)
                            bytesToRecv = USB_BLOCK_SIZE;

                        bytesTransferred = 0;
                        retVal = rcm_usb_device_read_ep1_out_sync(usbBuffer, bytesToRecv, &bytesTransferred);
                        if (retVal != 0)
                        {
                            printk("\nError %d trying to RECV data from host (xfer'd %u bytes), breaking.\n\n", retVal, bytesTransferred);
                            break;
                        }

                        memcpy(currMemAddr, usbBuffer, bytesTransferred);
                        currMemAddr += bytesTransferred;
                        int newProgressDots = ((u32)(currMemAddr)-startAddr)/progressDotBlockSize;
                        while (newProgressDots > numProgressDots)
                        {
                            video_puts(".");
                            numProgressDots++;
                        }
                    }
                    video_clear_line();
                    lastCommand = CMD_NONE;
                }
                else if (lastCommand == CMD_COPY && bytesTransferred == BYTES_TO_RECV[CMD_COPY]) 
                {
                    IniCopySection_t copySect;
                    copySect.sectname = "RCM";
                    copySect.compType = __builtin_bswap32(*(u32*)(&usbBuffer[0]));
                    copySect.src = __builtin_bswap32(*(u32*)(&usbBuffer[4]));
                    copySect.srclen = __builtin_bswap32(*(u32*)(&usbBuffer[8]));
                    copySect.dst = __builtin_bswap32(*(u32*)(&usbBuffer[12]));
                    copySect.dstlen = __builtin_bswap32(*(u32*)(&usbBuffer[16]));

                    execute_copy_section(&copySect);
                    lastCommand = CMD_NONE;
                }
                else if (lastCommand == CMD_BOOT && bytesTransferred == BYTES_TO_RECV[CMD_BOOT])
                {
                    IniBootSection_t bootSect;
                    bootSect.sectname = "RCM";
                    bootSect.pc = __builtin_bswap32(*(u32*)(&usbBuffer[0]));

                    execute_boot_section(&bootSect);
                    lastCommand = CMD_NONE;
                }
                else
                {
                    printk("\rUnknown command buffer received of len %u, contents: %02x%02x%02x%02x\n\n", bytesTransferred, 
                            (u32)usbBuffer[0], (u32)usbBuffer[1], (u32)usbBuffer[2], (u32)usbBuffer[3]);
                    video_clear_line();
                    lastCommand = CMD_NONE;
                }
            }
        }        
    }

    while (btn_read() != BTN_POWER) { sleep(10000); }
progend:
    // Tell the PMIC to turn everything off
    shutdown_using_pmic();

    /* Do nothing for now */
    return 0;
}
