#include <iostream>

#include <list>

#include <locale>
#include <ostream>
#include <sstream>
#include <algorithm>

#include <ctime>
#include <cstring>

#include <errno.h>
#include <string.h>

#include <fcntl.h>
#include <libgen.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <linux/types.h>
#include <linux/magic.h>

#define BTRFS_IOCTL_MAGIC 0x94

#define BTRFS_FIRST_FREE_OBJECTID 256ULL
#define BTRFS_LAST_FREE_OBJECTID -256ULL

#define BTRFS_SUBVOL_CREATE_ASYNC       (1ULL << 0)
#define BTRFS_SUBVOL_RDONLY             (1ULL << 1)
#define BTRFS_SUBVOL_QGROUP_INHERIT     (1ULL << 2)

#define BTRFS_PATH_NAME_MAX 4087
#define BTRFS_SUBVOL_NAME_MAX 4039

#define BTRFS_IOC_SNAP_CREATE      _IOW(BTRFS_IOCTL_MAGIC, 1, struct btrfs_ioctl_vol_args)
#define BTRFS_IOC_SNAP_DESTROY     _IOW(BTRFS_IOCTL_MAGIC, 15, struct btrfs_ioctl_vol_args)
#define BTRFS_IOC_START_SYNC       _IOR(BTRFS_IOCTL_MAGIC, 24, __u64)
#define BTRFS_IOC_WAIT_SYNC        _IOW(BTRFS_IOCTL_MAGIC, 22, __u64)
#define BTRFS_IOC_SUBVOL_GETFLAGS  _IOR(BTRFS_IOCTL_MAGIC, 25, __u64)
#define BTRFS_IOC_SUBVOL_SETFLAGS  _IOW(BTRFS_IOCTL_MAGIC, 26, __u64)

struct btrfs_ioctl_vol_args {
    __s64 fd;
    char name[BTRFS_PATH_NAME_MAX + 1];
};

bool is_dir(const char *dirname)
{
    struct stat st;

    if (stat(dirname, &st) != 0)
    {
        std::cout << "cannot access '" << dirname << "': " << strerror(errno) << std::endl;
        return false;
    }
    return !!S_ISDIR(st.st_mode);
}

bool is_subvol(const char *dirname)
{
    struct statfs stfs;
    struct stat st;

    if (statfs(dirname, &stfs) != 0)
    {
        std::cout << "cannot access '" << dirname << "': " << strerror(errno) << std::endl;
        return false;
    }
    if (stfs.f_type != BTRFS_SUPER_MAGIC)
    {
        std::cout << "not a btrfs filesystem: " << dirname << std::endl;
        return false;
    }
    if (stat(dirname, &st) != 0)
    {
        std::cout << "cannot access '" << dirname << "': " << strerror(errno) << std::endl;
        return false;
    }
    if (st.st_ino != BTRFS_FIRST_FREE_OBJECTID || !S_ISDIR(st.st_mode))
    {
        std::cout << "not subvolume" << std::endl;
        return false;
    }
    return true;
}

bool test_name(const char *name)
{
    return name[0] != '\0' && strchr(name, '/') == NULL && strcmp(name, ".") == 0 && strcmp(name, "..") == 0;
}

int open_dir(const char *dirname, DIR **dirstream)
{
    struct statfs stfs;
    struct stat st;

    //std::cout << "open_dir(" << dirname << ", " << (void *)dirstream << ")" << std::endl;

    if (statfs(dirname, &stfs) != 0)
    {
        std::cout << "cannot access '" << dirname << "': " << strerror(errno) << std::endl;
        return -1;
    }
    if (stfs.f_type != BTRFS_SUPER_MAGIC)
    {
        std::cout << "not a btrfs filesystem: " << dirname << std::endl;
        return -2;
    }
    if (stat(dirname, &st) != 0)
    {
        std::cout << "cannot access '" << dirname << "': " << strerror(errno) << std::endl;
        return -3;
    }
    if (!S_ISDIR(st.st_mode))
    {
        std::cout << "not a directory: " << dirname << std::endl;
        return -4;
    }

    *dirstream = opendir(dirname);

    if (*dirstream == NULL)
    {
        std::cout << "cannot open a directory: " << dirname << std::endl;
        return -5;
    }

    return dirfd(*dirstream);
}

int create(const char *src, const char *dst, const char *name)
{
    int dstfd;
    int srcfd;
    DIR *dstdir = NULL;
    DIR *srcdir = NULL;

    if (is_subvol(src) == false)
    {
        std::cout << "not subvolume" << std::endl;
        return -1;
    }
    if (is_dir(dst) == false)
    {
        std::cout << "'" << dst << "' it is not a directory" << std::endl;
        return -2;
    }
    if (test_name(name))
    {
        std::cout << "invalid snapshot name '" << name << "'" << std::endl;
        return -3;
    }

    dstfd = open_dir(dst, &dstdir);
    srcfd = open_dir(src, &srcdir);

    //std::cout << "src: " << (void *)srcdir << ", dst: " << (void *)dstdir << std::endl;

    if (dstdir && srcdir)
    {
        struct btrfs_ioctl_vol_args args;
        memset(&args, 0, sizeof(args));

        args.fd = srcfd;
        strcpy(args.name, name);

        if (ioctl(dstfd, BTRFS_IOC_SNAP_CREATE, &args) < 0)
        {
            std::cout << "cannot snapshot '" << src << "': " << strerror(errno) << std::endl;
        }
    }

    if (dstdir)
    {
        closedir(dstdir);
        close(dstfd);
    }
    if (srcdir)
    {
        closedir(srcdir);
        close(srcfd);
    }
    return 0;
}

int destroy(const char *dst)
{
    if (is_subvol(dst) == false)
    {
        std::cout << "'" << dst << "' is not subvolume" << std::endl;
        return -1;
    }

    char *cpath = realpath(dst, NULL);
    char *dupdname = strdup(cpath);
    char *dname = dirname(dupdname);
    char *dupvname = strdup(cpath);
    char *vname = basename(dupvname);

    free(cpath);

    DIR *dir = NULL;
    int fd = open_dir(dname, &dir);
    if (fd >= 0)
    {
        struct btrfs_ioctl_vol_args args = {};
        size_t len = strlen(vname);

        if (len >= sizeof(args.name))
        {
            std::cout << "name too long" << std::endl;
        }
        else
        {
            strcpy(args.name, vname);

            if (ioctl(fd, BTRFS_IOC_SNAP_DESTROY, &args) == -1)
            {
                std::cout << "Delete subvolume failed: " << strerror(errno) << std::endl;
            }
            std::cout << "Delete subvolume: '" << vname << "', ";

            uint64_t transid;
            if (ioctl(fd, BTRFS_IOC_START_SYNC, &transid) != -1)
            {
                std::cout << "wait trid: " << transid << "... ";

                if (ioctl(fd, BTRFS_IOC_WAIT_SYNC, &transid) != -1)
                {
                    std::cout << "done." << std::endl;
                }
                else
                {
                    std::cout << "failed." << std::endl;
                }
            }
        }

        closedir(dir);
        close(fd);
    }

    free(dupdname);
    free(dupvname);
    return 0;
}

bool set_readonly(const char *dirname, bool isset)
{
    int fd = open(dirname, O_RDONLY);
    if (fd < 0)
    {
        return false;
    }

    bool result;
    uint64_t flags;
    if (ioctl(fd, BTRFS_IOC_SUBVOL_GETFLAGS, &flags) == 0)
    {
        if (isset)
        {
            flags |= BTRFS_SUBVOL_RDONLY;
        }
        else
        {
            flags &= BTRFS_SUBVOL_RDONLY;
        }

        if (ioctl(fd, BTRFS_IOC_SUBVOL_SETFLAGS, &flags) == 0)
        {
            result = true;
        }
        else
        {
            result = false;
        }
    }
    else
    {
        result = false;
    }

    close(fd);
    return result;
}

bool test_directory(const char *dirname)
{
    struct stat st;

    if (stat(dirname, &st) != 0)
    {
        if (mkdir(dirname, 0755) != 0)
        {
            return false;
        }
        stat(dirname, &st);
    }
    return !!S_ISDIR(st.st_mode);
}

// https://www.safaribooksonline.com/library/view/c-cookbook/0596007612/ch05s03.html
std::ostream &formatDateTime(std::ostream &out, const tm &t, const char *fmt)
{
        const std::time_put<char> &writer = std::use_facet<std::time_put<char>>(out.getloc());

        int n = strlen(fmt);
        if (writer.put(out, out, ' ', &t, fmt, fmt + n).failed())
        {
                throw std::runtime_error("failure to format date time");
        }
        return out;
}

std::string toString(const tm &t, const char *format)
{
        std::stringstream s;

        formatDateTime(s, t, format);

        return s.str();
}

tm now()
{
        time_t t = time(0);
        return *localtime(&t);
}

bool remove_old_snapshots(std::string path, int count)
{
    DIR *dir = opendir(path.c_str());
    struct dirent *file;

    if (dir != NULL)
    {
        std::list<std::string> files;

        for (;;)
        {
            file = readdir(dir);
            if (file == NULL)
            {
                break;
            }
            if (strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0)
            {
                continue;
            }
            files.push_back(file->d_name);
        }
        closedir(dir);

        files.sort();

        //for (std::string filename : files)
        //{
        //    std::cout << filename << std::endl;
        //}
        //std::cout << "-" << std::endl;

        while (files.size() > count)
        {
            std::string file = files.front();
            std::string dst = path + "/" + file;

            destroy(dst.c_str());

            files.pop_front();
        }
    }
}

int main(int argc, char *argv[])
{
    char *csrc = NULL;
    char *cdst = NULL;
    char *ccnt = NULL;

    if (argc != 3 && argc != 4)
    {
        char *dup = strdup(argv[0]);
        char *self = basename(dup);

        std::cout << "./" << self << " source [destination] count" << std::endl;

        free(dup);
        return EXIT_FAILURE;
    }
    if (argc == 3)
    {
        csrc = realpath(argv[1], NULL);
        cdst = realpath(argv[1], NULL);
        ccnt = argv[2];
    }
    if (argc == 4)
    {
        csrc = realpath(argv[1], NULL);
        cdst = realpath(argv[2], NULL);
        ccnt = argv[3];
    }

    std::string src;
    std::string dst;
    std::string snapshot;

    if (csrc == NULL)
    {
        std::cout << "source is not exists" << std::endl;
        return -1;
    }
    if (cdst == NULL)
    {
        std::cout << "destination is not exists" << std::endl;
        return -1;
    }

    src = csrc;
    dst = cdst;
    free(csrc);
    free(cdst);
    csrc = NULL;
    cdst = NULL;

    if (is_dir(src.c_str()))
    {
        int count = -1;

        if (argc == 3)
        {
            dst += "/.snapshots";
        }

        if (test_directory(dst.c_str()) == false)
        {
            std::cerr << "cannot create snapshots directory" << std::endl;
            return EXIT_FAILURE;
        }

        try
        {
            snapshot = toString(now(), "%Z-%Y.%m.%d-%H.%M.%S");
        }
        catch(...)
        {
            std::cerr << "failed to format date time" << std::endl;
            return EXIT_FAILURE;
        }

        try
        {
            count = std::stoi(ccnt);

            if (count < 0)
            {
                std::cerr << "invalid argument" << std::endl;
                return EXIT_FAILURE;
            }
        }
        catch(...)
        {
            std::cerr << "invalid argument" << std::endl;
            return EXIT_FAILURE;
        }

        std::string dstpath = dst + "/" + snapshot;
        std::cout << "create snapshot" << std::endl;
        std::cout << " - src: " << src << std::endl;
        std::cout << " - dst: " << dstpath << std::endl;

        create(src.c_str(), dst.c_str(), snapshot.c_str());

        std::cout << " - set to readonly ";
        if (set_readonly(dstpath.c_str(), true))
        {
            std::cout << "success" << std::endl;
        }
        else
        {
            std::cout << "failed" << std::endl;
        }

        remove_old_snapshots(dst, count);
    }
    return EXIT_SUCCESS;
}
