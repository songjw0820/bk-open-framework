from pathlib import Path
import copy

import pytest

from tools.manifest import validate_manifest


ROOT = Path(__file__).resolve().parents[2]


def read_profile(name):
    from tools.manifest import load_manifest

    return load_manifest(ROOT / "platforms" / f"{name}.yaml")


@pytest.mark.parametrize("name", ["bk7259_minimal", "bk7259_robot"])
def test_supported_profiles_are_valid(name):
    validate_manifest(read_profile(name))


def test_missing_sdk_sha_is_rejected():
    manifest = copy.deepcopy(read_profile("bk7259_minimal"))
    del manifest["sdk"]["sha"]
    with pytest.raises(ValueError, match=r"sdk\.sha"):
        validate_manifest(manifest)


def test_sdk_and_solution_refs_must_match():
    manifest = copy.deepcopy(read_profile("bk7259_robot"))
    manifest["solution"]["ref"] = "main"
    with pytest.raises(ValueError, match="same release ref"):
        validate_manifest(manifest)


def test_unsupported_chip_is_rejected():
    manifest = copy.deepcopy(read_profile("bk7259_minimal"))
    manifest["chip"] = "BK7239"
    with pytest.raises(ValueError, match="unsupported chip"):
        validate_manifest(manifest)


def test_unsupported_board_is_rejected():
    manifest = copy.deepcopy(read_profile("bk7259_minimal"))
    manifest["board"] = "unknown_board"
    with pytest.raises(ValueError, match="unsupported board"):
        validate_manifest(manifest)
