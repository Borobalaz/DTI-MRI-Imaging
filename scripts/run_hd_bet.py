#!/usr/bin/env python3
"""
Run HD-BET on an MRI volume and save outputs next to the original file.

Example:
    python scripts/run_hd_bet.py --input C:/data/test_mri.nii.gz
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np

try:
    import nibabel as nib
except ImportError as exc:  # pragma: no cover - runtime dependency guard
    nib = None
    _NIB_IMPORT_ERROR = exc
else:
    _NIB_IMPORT_ERROR = None


def nifti_stem(path: Path) -> str:
    """Return a stem that handles .nii.gz correctly."""
    name = path.name
    if name.endswith(".nii.gz"):
        return name[:-7]
    return path.stem


def build_output_base(input_path: Path) -> Path:
    """Build HD-BET output base path next to the input MRI file."""
    stem = nifti_stem(input_path)
    if stem.endswith("_hdbet"):
        return input_path.with_name(stem)
    return input_path.with_name(f"{stem}_hdbet")


def resolve_hd_bet_command() -> str:
    """Resolve the HD-BET executable name."""
    exe = shutil.which("hd-bet")
    if exe:
        return exe

    # Fallback for some installs where command may be hd_bet.
    exe = shutil.which("hd_bet")
    if exe:
        return exe

    raise FileNotFoundError(
        "HD-BET executable not found. Install it first and ensure 'hd-bet' "
        "is available on PATH."
    )


def load_bvals(path: Path) -> np.ndarray:
    text = path.read_text(encoding="utf-8")
    values = [float(v) for v in text.split() if v.strip()]
    if not values:
        raise ValueError(f"No b-values found in file: {path}")
    return np.asarray(values, dtype=np.float32)


def infer_bvals_path(input_path: Path) -> Path | None:
    stem = nifti_stem(input_path)
    candidates = [
        input_path.with_name(f"{stem}.bval"),
        input_path.with_name(f"{stem}.bvals"),
        input_path.with_name("bvals"),
        input_path.with_name("bval"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


def build_s0_from_4d(
    input_path: Path,
    bvals_path: Path | None,
    b0_threshold: float,
) -> tuple[Path, Path, Path]:
    if nib is None:
        raise ImportError(
            "This script needs nibabel for 4D inputs. Install with: pip install nibabel"
        ) from _NIB_IMPORT_ERROR

    img = nib.load(str(input_path))
    data = img.get_fdata(dtype=np.float32)
    if data.ndim != 4:
        raise ValueError("Expected a 4D input image for S0 extraction.")

    frame_count = data.shape[3]
    b0_indices: np.ndarray

    if bvals_path is not None:
        bvals = load_bvals(bvals_path)
        if bvals.shape[0] != frame_count:
            raise ValueError(
                f"b-values count ({bvals.shape[0]}) does not match frame count ({frame_count})."
            )
        b0_indices = np.where(bvals <= b0_threshold)[0]
        if b0_indices.size == 0:
            print(
                f"No b0 frames found at threshold {b0_threshold}. Falling back to frame 0.",
                file=sys.stderr,
            )
            b0_indices = np.asarray([0], dtype=np.int64)
    else:
        print(
            "No bvals file provided/found. Falling back to frame 0 as S0 estimate.",
            file=sys.stderr,
        )
        b0_indices = np.asarray([0], dtype=np.int64)

    s0 = np.mean(data[..., b0_indices], axis=3, dtype=np.float32)

    stem = nifti_stem(input_path)
    s0_path = input_path.with_name(f"{stem}_s0_for_hdbet.nii.gz")
    s0_img = nib.Nifti1Image(s0, affine=img.affine, header=img.header)
    nib.save(s0_img, str(s0_path))

    return s0_path, input_path, input_path.with_name(f"{stem}_hdbet_masked4d.nii.gz")


def apply_mask_to_4d(input_4d: Path, mask_path: Path, output_path: Path) -> None:
    if nib is None:
        raise ImportError(
            "This script needs nibabel for 4D masking. Install with: pip install nibabel"
        ) from _NIB_IMPORT_ERROR

    dwi_img = nib.load(str(input_4d))
    dwi = dwi_img.get_fdata(dtype=np.float32)
    if dwi.ndim != 4:
        raise ValueError("Expected 4D DWI input for mask application.")

    mask_img = nib.load(str(mask_path))
    mask = mask_img.get_fdata(dtype=np.float32)
    if mask.ndim != 3:
        raise ValueError("Expected a 3D mask from HD-BET.")

    if dwi.shape[:3] != mask.shape:
        raise ValueError(
            f"Mask shape {mask.shape} does not match DWI spatial shape {dwi.shape[:3]}."
        )

    mask_bool = mask > 0.5
    masked = dwi * mask_bool[..., np.newaxis]
    out_img = nib.Nifti1Image(masked.astype(np.float32), affine=dwi_img.affine, header=dwi_img.header)
    nib.save(out_img, str(output_path))


def run_hd_bet(
    input_path: Path,
    device: str,
    tta: bool,
    overwrite: bool,
) -> tuple[Path, Path]:
    """Run HD-BET and return expected output image and mask paths."""
    output_base = build_output_base(input_path)
    output_image = Path(f"{output_base}.nii.gz")
    # HD-BET (this installed version) writes predicted mask as "*_bet.nii.gz".
    output_mask = Path(f"{output_base}_bet.nii.gz")

    # The installed hd-bet CLI supports: -i, -o, -device, --disable_tta, --save_bet_mask.
    cmd = [
        resolve_hd_bet_command(),
        "-i",
        str(input_path),
        "-o",
        str(output_image),
        "-device",
        device,
        "--save_bet_mask",
    ]

    if not tta:
        cmd.append("--disable_tta")

    if overwrite:
        for existing in (output_image, output_mask):
            if existing.exists():
                existing.unlink()

    print("Running:", " ".join(cmd))
    subprocess.run(cmd, check=True)

    # HD-BET output naming varies slightly across versions/builds.
    mask_candidates = [
        output_mask,
        output_image.with_name(f"{nifti_stem(output_image)}_bet.nii.gz"),
        output_image.with_name(f"{nifti_stem(output_image)}_mask.nii.gz"),
        input_path.with_name(f"{nifti_stem(input_path)}_bet.nii.gz"),
        input_path.with_name(f"{nifti_stem(input_path)}_mask.nii.gz"),
        input_path.with_name(f"{nifti_stem(input_path)}_hdbet_mask.nii.gz"),
    ]
    resolved_mask = next((p for p in mask_candidates if p.exists()), None)
    if resolved_mask is None:
        nearby_masks = sorted(input_path.parent.glob("*mask*.nii.gz"))
        nearby = "\n".join(str(p) for p in nearby_masks)
        raise FileNotFoundError(
            "HD-BET finished but no mask file was found. "
            f"Expected one of: {', '.join(str(p) for p in mask_candidates)}\n"
            f"Masks found in folder:\n{nearby if nearby else '(none)'}"
        )

    # If output image name differs by CLI version, keep the original path if present;
    # otherwise try common alternatives.
    image_candidates = [
        output_image,
        input_path.with_name(f"{nifti_stem(input_path)}_hdbet.nii.gz"),
        input_path.with_name(f"{nifti_stem(input_path)}.nii.gz"),
    ]
    resolved_image = next((p for p in image_candidates if p.exists()), output_image)

    return resolved_image, resolved_mask


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run HD-BET brain extraction and save output next to input MRI.",
    )
    parser.add_argument(
        "--input",
        required=True,
        type=Path,
        help="Path to input MRI (.nii or .nii.gz).",
    )
    parser.add_argument(
        "--device",
        default="cpu",
        choices=["cpu", "cuda", "mps"],
        help="Compute device for HD-BET.",
    )
    parser.add_argument(
        "--tta",
        action="store_true",
        help="Enable test-time augmentation (slower, potentially better).",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite existing HD-BET outputs if present.",
    )
    parser.add_argument(
        "--bvals",
        type=Path,
        default=None,
        help="Path to bvals file for 4D DWI (optional, auto-detected if omitted).",
    )
    parser.add_argument(
        "--b0-threshold",
        type=float,
        default=50.0,
        help="b-value threshold to classify b0 frames (used for 4D inputs).",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    input_path: Path = args.input.expanduser().resolve()

    if not input_path.exists():
        print(f"Input file not found: {input_path}", file=sys.stderr)
        return 1

    if not (input_path.suffix == ".nii" or input_path.name.endswith(".nii.gz")):
        print(
            "Input must be a NIfTI file (.nii or .nii.gz).",
            file=sys.stderr,
        )
        return 1

    run_input = input_path
    input_4d_for_masking: Path | None = None
    output_masked_4d: Path | None = None

    if nib is None:
        print(
            "Missing dependency 'nibabel'. Install with: pip install nibabel",
            file=sys.stderr,
        )
        return 3

    try:
        probe = nib.load(str(input_path))
        if probe.ndim == 4:
            bvals_path = args.bvals.expanduser().resolve() if args.bvals else infer_bvals_path(input_path)
            run_input, input_4d_for_masking, output_masked_4d = build_s0_from_4d(
                input_path=input_path,
                bvals_path=bvals_path,
                b0_threshold=args.b0_threshold,
            )
            print(f"4D input detected. Using S0 volume for HD-BET: {run_input}")
            if bvals_path is not None:
                print(f"Using b-values from: {bvals_path}")

        output_image, output_mask = run_hd_bet(
            input_path=run_input,
            device=args.device,
            tta=args.tta,
            overwrite=args.overwrite,
        )

        if input_4d_for_masking is not None and output_masked_4d is not None:
            apply_mask_to_4d(input_4d_for_masking, output_mask, output_masked_4d)
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 2
    except (ImportError, ValueError) as exc:
        print(str(exc), file=sys.stderr)
        return 3
    except subprocess.CalledProcessError as exc:
        print(f"HD-BET failed with exit code {exc.returncode}", file=sys.stderr)
        return exc.returncode

    print("Done.")
    print(f"Brain-extracted image: {output_image}")
    print(f"Brain mask: {output_mask}")
    if output_masked_4d is not None:
        print(f"Masked 4D DWI: {output_masked_4d}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
