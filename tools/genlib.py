"""Shared low-level helpers for the asset generator scripts.

Same convention as gen-grid-image.ps1 on the sprite side: each generator
stays explicit and asset-specific, and only genuinely generic plumbing
lives here.
"""
import os
import tempfile


def repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def validated_out_dir(out_dir):
    """Canonicalize a caller-supplied output dir and require it to stay
    inside the repo or the system temp dir, so a bad CLI argument can't
    write outside those roots."""
    resolved = os.path.realpath(out_dir)
    allowed = (os.path.realpath(repo_root()),
               os.path.realpath(tempfile.gettempdir()))
    for root in allowed:
        if resolved == root or resolved.startswith(root + os.sep):
            return resolved
    raise ValueError(f'output dir {out_dir!r} must be inside one of {allowed}')


def resolve_out_dir(out_dir, *default_repo_relative_parts):
    """Default to a repo-relative path when no dir is given; validate the
    caller's path otherwise."""
    if out_dir is None:
        return os.path.join(repo_root(), *default_repo_relative_parts)
    return validated_out_dir(out_dir)
