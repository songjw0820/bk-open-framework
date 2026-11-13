"""Validation for BK Open Framework platform manifests.

The repository intentionally keeps this validator dependency-free so it can run
before the SDK toolchain is installed.
"""

from __future__ import annotations

import argparse
import ast
import re
from pathlib import Path
from typing import Any


SUPPORTED_CHIPS = {"BK7259"}
SUPPORTED_BOARDS = {"bk7259_robot"}
SHA_RE = re.compile(r"^[0-9a-fA-F]{40}$")


def _scalar(value: str) -> Any:
    value = value.strip()
    if not value or value in {"null", "Null", "NULL", "~"}:
        return None
    if value in {"true", "True"}:
        return True
    if value in {"false", "False"}:
        return False
    if value.startswith(("'", '"')):
        try:
            return ast.literal_eval(value)
        except (SyntaxError, ValueError):
            return value[1:-1]
    if value.startswith("[") and value.endswith("]"):
        inner = value[1:-1].strip()
        return [] if not inner else [_scalar(item) for item in inner.split(",")]
    return value


def _strip_comment(line: str) -> str:
    # Profiles contain URLs but no quoted '#' values; only treat a hash after
    # whitespace as a comment marker.
    return re.sub(r"\s+#.*$", "", line).rstrip()


def _parse_block(lines: list[tuple[int, str]], index: int, indent: int) -> tuple[Any, int]:
    is_list = lines[index][1].startswith("-")
    result: Any = [] if is_list else {}
    while index < len(lines):
        current_indent, text = lines[index]
        if current_indent < indent:
            break
        if current_indent > indent:
            raise ValueError(f"invalid indentation near: {text}")
        if is_list:
            if not text.startswith("-"):
                break
            item = text[1:].strip()
            if item:
                result.append(_scalar(item))
                index += 1
            else:
                index += 1
                if index >= len(lines) or lines[index][0] <= indent:
                    result.append(None)
                else:
                    child, index = _parse_block(lines, index, lines[index][0])
                    result.append(child)
            continue
        if ":" not in text:
            raise ValueError(f"expected key/value near: {text}")
        key, raw = text.split(":", 1)
        key = key.strip()
        raw = raw.strip()
        index += 1
        if raw:
            result[key] = _scalar(raw)
        elif index < len(lines) and lines[index][0] > indent:
            child, index = _parse_block(lines, index, lines[index][0])
            result[key] = child
        else:
            result[key] = None
    return result, index


def load_manifest(path: str | Path) -> dict[str, Any]:
    path = Path(path)
    lines: list[tuple[int, str]] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        text = _strip_comment(raw)
        if text.strip():
            lines.append((len(text) - len(text.lstrip(" ")), text.lstrip()))
    if not lines:
        raise ValueError(f"manifest is empty: {path}")
    manifest, index = _parse_block(lines, 0, lines[0][0])
    if index != len(lines) or not isinstance(manifest, dict):
        raise ValueError(f"invalid manifest: {path}")
    return manifest


def _required(mapping: dict[str, Any], key: str, prefix: str) -> Any:
    if key not in mapping or mapping[key] in (None, ""):
        raise ValueError(f"missing required field: {prefix}.{key}")
    return mapping[key]


def _validate_dependency(dep: Any, prefix: str) -> None:
    if not isinstance(dep, dict):
        raise ValueError(f"{prefix} must be a mapping or null")
    for key in ("repository", "ref", "sha"):
        _required(dep, key, prefix)
    if not isinstance(dep["sha"], str) or not SHA_RE.fullmatch(dep["sha"]):
        raise ValueError(f"{prefix}.sha must be a 40-character hexadecimal commit")


def validate_manifest(manifest: dict[str, Any]) -> None:
    if not isinstance(manifest, dict):
        raise ValueError("manifest must be a mapping")
    chip = _required(manifest, "chip", "manifest")
    board = _required(manifest, "board", "manifest")
    _required(manifest, "profile", "manifest")
    if chip not in SUPPORTED_CHIPS:
        raise ValueError(f"unsupported chip: {chip}")
    if board not in SUPPORTED_BOARDS:
        raise ValueError(f"unsupported board: {board}")
    sdk = _required(manifest, "sdk", "manifest")
    _validate_dependency(sdk, "sdk")
    solution = manifest.get("solution")
    if solution is not None:
        _validate_dependency(solution, "solution")
        if solution["ref"] != sdk["ref"]:
            raise ValueError("sdk.ref and solution.ref must use the same release ref")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="validate BK Open Framework manifests")
    parser.add_argument("manifest", nargs="+", type=Path)
    args = parser.parse_args(argv)
    for path in args.manifest:
        try:
            validate_manifest(load_manifest(path))
        except (OSError, ValueError) as exc:
            parser.error(f"{path}: {exc}")
        print(f"valid: {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
