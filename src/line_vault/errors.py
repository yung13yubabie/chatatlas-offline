class VaultError(Exception):
    """Expected error whose message is safe to show to the user."""


class IntegrityError(VaultError):
    pass


class CapabilityError(VaultError):
    pass


class Cancelled(VaultError):
    pass


class DownloadUnavailable(VaultError):
    """Source explicitly reports expired or unavailable content; do not retry."""


class RetryableDownload(VaultError):
    """A temporary transfer failure, safe to retry after checking source identity."""
