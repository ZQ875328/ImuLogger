#include "Logging.h"

#include <fcntl.h>
#include <nuttx/config.h>
#include <stdio.h>
#include <sys/stat.h>

#include "Common_DebugPrint.h"
#include "PowerCtrl_public.h"

#define MAX_PATH_LENGTH (32)
#define MAX_FILE_SIZE   (1024 * 1024 * 1024) // 1GB

typedef struct tagLogging_Writer_t {
    uint32_t dirId;
    uint32_t fileId;
    uint32_t fileSize;
    char     filename[MAX_PATH_LENGTH];
    int      fd;
} Logging_Writer_t;

const static char* outDir = "/mnt/sd0/log";

static Logging_Writer_t logging_Writer_instance;

static Logging_Writer_t* GetInstance(void)
{
    return &logging_Writer_instance;
}

static int CreateTopDir(void)
{
    Logging_Writer_t* self = GetInstance();
    int ret;
    struct stat info;

    /* stat関数でパスの情報を取得 */
    if (stat(outDir, &info) == 0) {
        /* ディレクトリであるかを確認 */
        return S_ISDIR(info.st_mode);
    } else {
        ret = mkdir(outDir, 0666);
        if (ret == OK) {
            PRINT_INFO("Create directory: %s\n", outDir);
            return true;
        } else {
            if (errno != EEXIST) {
                PRINT_ERROR("mkdir err(errno:%d)\n", errno);
                return ERROR;
            }
        }
    }

    return OK;
}

static int CreateDir(void)
{
    Logging_Writer_t* self = GetInstance();
    int ret;
    struct stat info;

    for (int idx = 0; idx < 10000; ++idx) {
        snprintf(self->filename,
            MAX_PATH_LENGTH,
            "%s/%04d",
            outDir,
            idx
        );

        if (stat(self->filename, &info) == 0) {
            PRINT_DEBUG("File already exists: %s\n", self->filename);
            continue;
        } else if (errno == ENOENT) {
            self->dirId = idx;
            ret         = mkdir(self->filename, 0666);
            if (ret == OK) {
                PRINT_INFO("Create directory: %s\n", self->filename);
                return OK;
            } else {
                if (errno != EEXIST) {
                    PRINT_ERROR("mkdir err(errno:%d)\n", errno);
                    return ERROR;
                }
            }
        }
    }

    return ERROR;
} /* CreateDir */

static int OpenFile(void)
{
    Logging_Writer_t* self = GetInstance();
    struct stat tmp;

    snprintf(self->filename,
        MAX_PATH_LENGTH,
        "%s/%04d/%02d.bin",
        outDir,
        self->dirId,
        self->fileId
    );

    self->fd = creat(self->filename, 0644);
    if (self->fd < 0) {
        PRINT_ERROR("open err(%s) errno(%d)\n", self->filename, errno);
        return ERROR;
    }

    self->fileId++;
    self->fileSize = 0;

    return OK;
}

static int CloseFile(void)
{
    Logging_Writer_t* self = GetInstance();

    int ret = OK;

    if (self->fd >= 0) {
        PRINT_INFO("Close file: %s\n", self->filename);
        if (fsync(self->fd) < 0) {
            PRINT_ERROR("fsync err(%s) errno(%d)\n", self->filename, errno);
            ret = ERROR;
        }
        if (close(self->fd) < 0) {
            PRINT_ERROR("close err(%s) errno(%d)\n", self->filename, errno);
            ret = ERROR;
        }
        self->fd = -1;
    }

    return ret;
}

int Logging_Writer_Initialize(void)
{
    Logging_Writer_t* self = GetInstance();

    self->fd       = -1;
    self->dirId    = 0;
    self->fileId   = 0;
    self->fileSize = 0;

    if (CreateTopDir() == ERROR) {
        PRINT_ERROR("Failed to create top directory: %s\n", outDir);
        return ERROR;
    }

    if (CreateDir() == ERROR) {
        PRINT_ERROR("Failed to create directory\n");
        return ERROR;
    }

    if (OpenFile() == ERROR) {
        PRINT_ERROR("Failed to open file\n");
        return ERROR;
    }

    return OK;
}

int Logging_Writer_Write(void* data, size_t size)
{
    Logging_Writer_t* self = GetInstance();

    size_t ret = write(self->fd, data, size);

    if (true) {
        fsync(self->fd);
    }

    PRINT_DEBUG("Write data to file: %s, size: %zu %d\n", self->filename, size, ret);

    if (ret != size) {
        PRINT_ERROR("write err(%s)\n", self->filename);
        return ERROR;
    }
    self->fileSize += size;
    if (self->fileSize >= MAX_FILE_SIZE) {
        CloseFile();
        OpenFile();
    }

    return OK;
}

int Logging_Writer_Close(void)
{
    Logging_Writer_t* self = GetInstance();

    CloseFile();
    return OK;
}
