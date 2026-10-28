"""One archive operation at a time, including view publication and ZIP backup."""
import os
import stat
from pathlib import Path

from .errors import VaultError
from .safeio import within


class ArchiveLock:
    def __init__(self, root: Path):
        self.file = None
        path = within(root, ".vault-lock")
        flags = os.O_RDWR | os.O_CREAT | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_BINARY", 0)
        fd = os.open(path, flags, 0o600)
        try:
            if not stat.S_ISREG(os.fstat(fd).st_mode):
                raise OSError("not regular")
            self.file = os.fdopen(fd, "r+b", buffering=0)
        except BaseException:
            os.close(fd)
            raise
        try:
            if os.name == "nt":
                import msvcrt
                if os.fstat(self.file.fileno()).st_size == 0:
                    self.file.write(b"\0")
                self.file.seek(0)
                msvcrt.locking(self.file.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(self.file.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            self.file.close()
            self.file = None
            raise VaultError("這份封存正在使用中。請等另一項保存、檢查或備份完成後再試。") from None

    def close(self):
        if self.file:
            # Keep the inode: deleting a lock file can permit two concurrent locks.
            if os.name == "nt":
                import msvcrt
                self.file.seek(0)
                msvcrt.locking(self.file.fileno(), msvcrt.LK_UNLCK, 1)
            self.file.close()
            self.file = None
