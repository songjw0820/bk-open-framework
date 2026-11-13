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
    with pytest.raises(ValueError, match="canonical bk_solution_ai"):
        validate_manifest(manifest)


def test_sdk_repository_ref_and_sha_must_match_canonical_release():
    manifest = copy.deepcopy(read_profile("bk7259_minimal"))
    manifest["sdk"]["repository"] = "https://example.invalid/sdk.git"
    with pytest.raises(ValueError, match="canonical BK7259 SDK"):
        validate_manifest(manifest)


@pytest.mark.parametrize("field", ["ref", "sha"])
def test_sdk_canonical_release_fields_are_required(field):
    manifest = copy.deepcopy(read_profile("bk7259_minimal"))
    manifest["sdk"][field] = "main" if field == "ref" else "0" * 40
    with pytest.raises(ValueError, match="canonical BK7259 SDK"):
        validate_manifest(manifest)


def test_minimal_profile_requires_null_solution():
    manifest = copy.deepcopy(read_profile("bk7259_minimal"))
    manifest["solution"] = {"repository": "x", "ref": "x", "sha": "0" * 40}
    with pytest.raises(ValueError, match="minimal profile requires solution:null"):
        validate_manifest(manifest)


def test_robot_profile_requires_canonical_solution():
    manifest = copy.deepcopy(read_profile("bk7259_robot"))
    manifest["solution"] = None
    with pytest.raises(ValueError, match="robot profile requires solution"):
        validate_manifest(manifest)


def test_robot_solution_repository_and_sha_must_match_canonical_release():
    manifest = copy.deepcopy(read_profile("bk7259_robot"))
    manifest["solution"]["sha"] = "0" * 40
    with pytest.raises(ValueError, match="canonical bk_solution_ai"):
        validate_manifest(manifest)


def test_robot_solution_repository_must_match_canonical_release():
    manifest = copy.deepcopy(read_profile("bk7259_robot"))
    manifest["solution"]["repository"] = "https://example.invalid/solution.git"
    with pytest.raises(ValueError, match="canonical bk_solution_ai"):
        validate_manifest(manifest)


def test_robot_solution_ref_must_match_canonical_release():
    manifest = copy.deepcopy(read_profile("bk7259_robot"))
    manifest["solution"]["ref"] = "release/v4.0.1.10"
    with pytest.raises(ValueError, match="canonical bk_solution_ai"):
        validate_manifest(manifest)


@pytest.mark.parametrize("profile", ["debug", "production"])
def test_unsupported_profile_is_rejected(profile):
    manifest = copy.deepcopy(read_profile("bk7259_minimal"))
    manifest["profile"] = profile
    with pytest.raises(ValueError, match="unsupported profile"):
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
