import copy
from pathlib import Path
import tempfile
import unittest
import yaml

from tools.bk_manifest import (
    ManifestValidationError,
    load_manifest,
    validate_manifest_dict,
)


REPO_ROOT = Path(__file__).resolve().parents[2]
PLATFORMS_DIR = REPO_ROOT / "platforms"


class TestManifestValidation(unittest.TestCase):
    def test_minimal_manifest_valid_and_isolated_from_solution(self):
        manifest = load_manifest(PLATFORMS_DIR / "bk7259_minimal.yaml")
        self.assertEqual(manifest["name"], "bk7259_minimal")
        self.assertEqual(manifest["profile"], "minimal")
        self.assertIsNone(manifest["solution"])
        self.assertEqual(manifest["cp_template"]["source"], "sdk")
        self.assertEqual(manifest["cp_template"]["path"], "projects/app/cp")
        self.assertEqual(
            manifest["sdk"]["peeled_commit_sha"],
            "584b947920c414230f9909375f008e7d6a2c6c6b",
        )
        self.assertNotIn("bk_ota", manifest["components"])

    def test_robot_manifest_valid_and_defers_bk_ota(self):
        manifest = load_manifest(PLATFORMS_DIR / "bk7259_robot.yaml")
        self.assertEqual(manifest["name"], "bk7259_robot")
        self.assertEqual(manifest["profile"], "robot")
        self.assertIsNotNone(manifest["solution"])
        self.assertEqual(manifest["sdk"]["ref"], manifest["solution"]["ref"])
        self.assertEqual(manifest["cp_template"]["source"], "solution")
        self.assertEqual(manifest["cp_template"]["path"], "projects/beken_robot/cp")
        self.assertNotIn("bk_ota", manifest["components"])
        self.assertIn("bk_ota", manifest.get("deferred_target_components", []))

    def test_rejects_ambiguous_sha_field(self):
        raw = yaml.safe_load((PLATFORMS_DIR / "bk7259_minimal.yaml").read_text(encoding="utf-8"))
        bad = copy.deepcopy(raw)
        bad["sdk"]["sha"] = bad["sdk"]["peeled_commit_sha"]
        with self.assertRaises(ManifestValidationError) as ctx:
            validate_manifest_dict(bad)
        self.assertIn("ambiguous 'sha'", str(ctx.exception))

    def test_rejects_minimal_profile_referencing_solution(self):
        raw = yaml.safe_load((PLATFORMS_DIR / "bk7259_minimal.yaml").read_text(encoding="utf-8"))
        bad = copy.deepcopy(raw)
        bad["solution"] = {
            "repository": "https://github.com/bekencorp/bk_solution_ai.git",
            "ref": "release/v4.0.1.11",
            "tag_object_sha": "009c95182de07ae874e39e3d3c24994431bcec70",
            "peeled_commit_sha": "a1e865e156e8dc31dfb360219c33a6a6d908ea98",
        }
        with self.assertRaises(ManifestValidationError) as ctx:
            validate_manifest_dict(bad)
        self.assertIn("minimal profile must not depend on solution", str(ctx.exception))

    def test_rejects_minimal_profile_with_solution_cp_template(self):
        raw = yaml.safe_load((PLATFORMS_DIR / "bk7259_minimal.yaml").read_text(encoding="utf-8"))
        bad = copy.deepcopy(raw)
        bad["cp_template"]["source"] = "solution"
        with self.assertRaises(ManifestValidationError) as ctx:
            validate_manifest_dict(bad)
        self.assertIn("minimal profile cp_template.source must be 'sdk'", str(ctx.exception))

    def test_rejects_mismatched_verification_sha(self):
        raw = yaml.safe_load((PLATFORMS_DIR / "bk7259_robot.yaml").read_text(encoding="utf-8"))
        bad = copy.deepcopy(raw)
        bad["verification"]["sdk_peeled_commit_sha"] = "0" * 40
        with self.assertRaises(ManifestValidationError) as ctx:
            validate_manifest_dict(bad)
        self.assertIn("verification.sdk_peeled_commit_sha", str(ctx.exception))


if __name__ == "__main__":
    unittest.main()
