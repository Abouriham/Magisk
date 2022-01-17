#include <sys/mount.h>

#include <magisk.hpp>
#include <utils.hpp>
#include "../zygisk.hpp"

#include "deny.hpp"

using namespace std;

static void lazy_unmount(const char* mountpoint) {
    if (umount2(mountpoint, MNT_DETACH) != -1)
        LOGD("denylist: Unmounted (%s)\n", mountpoint);
}

#define TMPFS_MNT(dir) (mentry->mnt_type == "tmpfs"sv && str_starts(mentry->mnt_dir, "/" #dir))

void revert_unmount() {
    vector<string> targets;

    // Unmount dummy skeletons and MAGISKTMP
    targets.push_back(MAGISKTMP);
    parse_mnt("/proc/self/mounts", [&](mntent *mentry) {
        if (TMPFS_MNT(system) || TMPFS_MNT(vendor) || TMPFS_MNT(product) || TMPFS_MNT(system_ext))
            targets.emplace_back(mentry->mnt_dir);
        return true;
    });

    for (auto &s : reversed(targets))
        lazy_unmount(s.data());
    targets.clear();

    // Unmount all Magisk created mounts
    parse_mnt("/proc/self/mounts", [&](mntent *mentry) {
        if (str_contains(mentry->mnt_fsname, BLOCKDIR))
            targets.emplace_back(mentry->mnt_dir);
        return true;
    });

    for (auto &s : reversed(targets))
        lazy_unmount(s.data());
}

void enter_clean_ns() {
    if (int fd = connect_daemon(); fd >= 0) {
        write_int(fd, ZYGISK_REQUEST);
        write_int(fd, ZYGISK_GET_CLEAN_NS);
        auto clean_ns = recv_fd(fd);
        LOGD("denylist: set to clean ns %d\n", clean_ns);
        if (clean_ns > 0) xsetns(clean_ns, CLONE_NEWNS);
        close(fd);
        close(clean_ns);
    }
}
