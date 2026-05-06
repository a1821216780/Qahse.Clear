from __future__ import annotations

from pathlib import Path
from typing import Iterable

from openpyxl import Workbook
from openpyxl.styles import Font


ROOT = Path(__file__).resolve().parent
OUT_DIR = ROOT / "batch_templates"

NREL_5MW = {
    "Turbine": "NREL 5MW",
    "HubHeight_m": 90.0,
    "RotorDiameter_m": 126.0,
    "IECClass": "Class_I",
    "TurbulenceClass": "Class_B",
    "CutIn_mps": 3.0,
    "Rated_mps": 11.4,
    "CutOut_mps": 25.0,
}

OPERATING_SPEEDS = [4.0, 6.0, 8.0, 10.0, 11.4, 12.0, 14.0, 16.0, 18.0, 20.0, 22.0, 24.0]
EVENT_SPEEDS = [8.0, 11.4, 16.0, 20.0, 24.0]
EWM_SPEEDS = [11.4, 25.0]


def fmt_speed(speed: float) -> str:
    text = f"{speed:.1f}"
    if text.endswith(".0"):
        text = text[:-2]
    return text.replace(".", "p")


def with_sheet_header(ws, headers: list[str]) -> None:
    for idx, header in enumerate(headers, start=1):
        cell = ws.cell(1, idx, header)
        cell.font = Font(bold=True)
    ws.freeze_panes = "A2"


def add_row(rows: list[list[str]], headers: list[str], mapping: dict[str, object]) -> None:
    row: list[str] = []
    for key in headers:
        value = mapping.get(key, "")
        if isinstance(value, bool):
            row.append("true" if value else "false")
        elif value is None:
            row.append("")
        else:
            row.append(str(value))
    rows.append(row)


def iec_fixed_headers() -> list[str]:
    return [
        "CaseName",
        "Enabled",
        "OutputSubdir",
        "Override.WindModel",
        "Override.TurbModel",
        "Override.MeanWindSpeed",
        "Override.EWMType",
        "Override.EventSign",
        "Override.RandSeed",
        "Override.WrTrbts",
        "Override.WrBlwnd",
        "Override.WrTrwnd",
        "Meta.Standard",
        "Meta.Turbine",
        "Meta.StructureType",
        "Meta.DLC",
        "Meta.WindFamily",
        "Meta.WaterDepth",
        "Meta.WaveHs",
        "Meta.WaveTp",
        "Meta.CurrentSpeed",
    ]


def iec_float_headers() -> list[str]:
    headers = iec_fixed_headers()
    insert_at = headers.index("Meta.WaterDepth") + 1
    headers.insert(insert_at, "Meta.PlatformMotion")
    return headers


def build_iec_rows(structure: str) -> tuple[list[str], list[list[str]]]:
    headers = iec_float_headers() if structure == "floating" else iec_fixed_headers()
    rows: list[list[str]] = []
    standard = "IEC 61400-3-2" if structure == "floating" else "IEC 61400-3-1"
    water_depth = "120" if structure == "floating" else "35"
    platform_motion = "surge/heave/pitch" if structure == "floating" else ""

    def meta_base(dlc: str, family: str, speed: float) -> dict[str, object]:
        base = {
            "Enabled": True,
            "Override.TurbModel": "IEC_KAIMAL",
            "Override.MeanWindSpeed": speed,
            "Override.WrTrbts": True,
            "Override.WrBlwnd": True,
            "Override.WrTrwnd": False,
            "Meta.Standard": standard,
            "Meta.Turbine": NREL_5MW["Turbine"],
            "Meta.StructureType": structure,
            "Meta.DLC": dlc,
            "Meta.WindFamily": family,
            "Meta.WaterDepth": water_depth,
            "Meta.WaveHs": f"{2.0 + 0.12 * speed:.1f}",
            "Meta.WaveTp": f"{7.0 + 0.18 * speed:.1f}",
            "Meta.CurrentSpeed": f"{0.4 + 0.02 * speed:.2f}",
        }
        if structure == "floating":
            base["Meta.PlatformMotion"] = platform_motion
        return base

    for speed in OPERATING_SPEEDS:
        for seed_idx in range(1, 7):  # IEC requires 6 seeds per wind speed
            seed_suffix = f"_S{seed_idx:02d}"
            add_row(rows, headers, {
                "CaseName": f"NREL5MW_DLC1p1_NTM_U{fmt_speed(speed)}{seed_suffix}",
                "OutputSubdir": f"DLC1p1_NTM/U{fmt_speed(speed)}/S{seed_idx:02d}",
                "Override.WindModel": "NTM",
                "Override.RandSeed": 12345 + seed_idx,
                **meta_base("1.1", "NTM", speed),
            })
            add_row(rows, headers, {
                "CaseName": f"NREL5MW_DLC1p3_ETM_U{fmt_speed(speed)}{seed_suffix}",
                "OutputSubdir": f"DLC1p3_ETM/U{fmt_speed(speed)}/S{seed_idx:02d}",
                "Override.WindModel": "ETM",
                "Override.RandSeed": 22345 + seed_idx,
                **meta_base("1.3", "ETM", speed),
            })
        for seed_idx in range(1, 7):
            seed_suffix = f"_S{seed_idx:02d}"
            add_row(rows, headers, {
                "CaseName": f"NREL5MW_DLC2p3_EOG_U{fmt_speed(speed)}{seed_suffix}",
                "OutputSubdir": f"DLC2p3_EOG/U{fmt_speed(speed)}/S{seed_idx:02d}",
                "Override.WindModel": "EOG",
                "Override.RandSeed": 52345 + seed_idx,
                **meta_base("2.3", "EOG", speed),
            })

    for speed in EVENT_SPEEDS:
        for sign_name in ("POSITIVE", "NEGATIVE"):
            sign_suffix = "Pos" if sign_name == "POSITIVE" else "Neg"
            for seed_idx in range(1, 7):
                seed_suffix_full = f"_{sign_suffix}_S{seed_idx:02d}"
                for family, dlc_num in [("EDC", "2.3"), ("ECD", "2.3"), ("EWS", "2.3")]:
                    add_row(rows, headers, {
                        "CaseName": f"NREL5MW_DLC{dlc_num}_{family}{seed_suffix_full}_U{fmt_speed(speed)}",
                        "OutputSubdir": f"DLC{dlc_num}_{family}/{sign_suffix}/U{fmt_speed(speed)}/S{seed_idx:02d}",
                        "Override.WindModel": family,
                        "Override.EventSign": sign_name,
                        "Override.RandSeed": 42345 + seed_idx,
                        **meta_base(dlc_num, family, speed),
                    })

    for speed in EWM_SPEEDS:
        for seed_idx in range(1, 7):
            seed_suffix = f"_S{seed_idx:02d}"
            for return_name, ewm_type, dlc_num in [
                ("EWM1", "Turbulent",  "6.3"),
                ("EWM1", "Steady",     "6.4"),
                ("EWM50", "Turbulent", "6.1"),
                ("EWM50", "Steady",    "6.2"),
            ]:
                add_row(rows, headers, {
                    "CaseName": f"NREL5MW_DLC{dlc_num}_{return_name}_{ewm_type}_U{fmt_speed(speed)}{seed_suffix}",
                    "OutputSubdir": f"DLC{dlc_num}_{return_name}/{ewm_type}/U{fmt_speed(speed)}/S{seed_idx:02d}",
                    "Override.WindModel": return_name,
                    "Override.EWMType": ewm_type,
                    "Override.RandSeed": 32345 + seed_idx,
                    **meta_base(dlc_num, return_name, speed),
                })

    add_row(rows, headers, {
        "CaseName": "NREL5MW_UNIFORM_U11p4",
        "OutputSubdir": "UNIFORM/U11p4",
        "Override.WindModel": "UNIFORM",
        **meta_base("1.1", "UNIFORM", 11.4),
    })

    return headers, rows


def build_generic_rows() -> tuple[list[str], list[list[str]]]:
    headers = [
        "CaseName",
        "Enabled",
        "OutputSubdir",
        "Override.WindModel",
        "Override.TurbModel",
        "Override.MeanWindSpeed",
        "Override.EWMType",
        "Override.EventSign",
        "Override.WrTrbts",
        "Override.WrBlwnd",
        "Override.WrTrwnd",
        "Override.TI_u",
        "Override.TI_v",
        "Override.TI_w",
        "Override.Latitude",
        "Meta.Project",
        "Meta.Turbine",
        "Meta.Site",
        "Meta.WaterDepth",
        "Meta.Bundle",
        "Meta.Note",
    ]
    rows: list[list[str]] = []

    def base(bundle: str, note: str, speed: float = 11.4) -> dict[str, object]:
        return {
            "Enabled": True,
            "Override.MeanWindSpeed": speed,
            "Override.WrTrbts": True,
            "Override.WrBlwnd": True,
            "Override.WrTrwnd": False,
            "Meta.Project": "WindL batch template",
            "Meta.Turbine": NREL_5MW["Turbine"],
            "Meta.Site": "Offshore generic",
            "Meta.WaterDepth": "45",
            "Meta.Bundle": bundle,
            "Meta.Note": note,
        }

    spectral_cases = [
        ("IEC_KAIMAL", "IEC Kaimal baseline"),
        ("IEC_VKAIMAL", "IEC von Karman baseline"),
        ("B_KAL", "Bladed Kaimal audit"),
        ("B_VKAL", "Bladed von Karman audit"),
        ("B_IVKAL", "Bladed improved von Karman audit"),
        ("B_MANN", "Mann spectral tensor audit"),
    ]
    for turb_model, note in spectral_cases:
        add_row(rows, headers, {
            "CaseName": f"NREL5MW_{turb_model}_NTM_U11p4",
            "OutputSubdir": f"Spectrum/{turb_model}",
            "Override.WindModel": "NTM",
            "Override.TurbModel": turb_model,
            "Override.TI_u": 18 if turb_model == "B_MANN" else "",
            "Override.TI_v": 13 if turb_model == "B_MANN" else "",
            "Override.TI_w": 8 if turb_model == "B_MANN" else "",
            "Override.Latitude": 54.0 if turb_model == "B_IVKAL" else "",
            **base("SpectrumAudit", note),
        })

    for speed in (8.0, 11.4, 16.0, 22.0):
        add_row(rows, headers, {
            "CaseName": f"NREL5MW_NTM_U{fmt_speed(speed)}",
            "OutputSubdir": f"NTM/U{fmt_speed(speed)}",
            "Override.WindModel": "NTM",
            "Override.TurbModel": "IEC_KAIMAL",
            **base("Operating", "Operating turbulence sweep", speed),
        })
        add_row(rows, headers, {
            "CaseName": f"NREL5MW_ETM_U{fmt_speed(speed)}",
            "OutputSubdir": f"ETM/U{fmt_speed(speed)}",
            "Override.WindModel": "ETM",
            "Override.TurbModel": "IEC_KAIMAL",
            **base("Operating", "Extreme turbulence sweep", speed),
        })
        add_row(rows, headers, {
            "CaseName": f"NREL5MW_EOG_U{fmt_speed(speed)}",
            "OutputSubdir": f"EOG/U{fmt_speed(speed)}",
            "Override.WindModel": "EOG",
            "Override.TurbModel": "IEC_KAIMAL",
            **base("Events", "Extreme operating gust", speed),
        })

    for family in ("EDC", "ECD", "EWS"):
        for sign_name in ("POSITIVE", "NEGATIVE"):
            sign_suffix = "Pos" if sign_name == "POSITIVE" else "Neg"
            add_row(rows, headers, {
                "CaseName": f"NREL5MW_{family}_{sign_suffix}_U11p4",
                "OutputSubdir": f"{family}/{sign_suffix}",
                "Override.WindModel": family,
                "Override.TurbModel": "IEC_KAIMAL",
                "Override.EventSign": sign_name,
                **base("Events", f"{family} event", 11.4),
            })

    for return_name in ("EWM1", "EWM50"):
        for ewm_type in ("Steady", "Turbulent"):
            add_row(rows, headers, {
                "CaseName": f"NREL5MW_{return_name}_{ewm_type}",
                "OutputSubdir": f"{return_name}/{ewm_type}",
                "Override.WindModel": return_name,
                "Override.TurbModel": "IEC_KAIMAL",
                "Override.EWMType": ewm_type,
                **base("ExtremeWind", f"{return_name} {ewm_type.lower()}", 25.0),
            })

    add_row(rows, headers, {
        "CaseName": "NREL5MW_UNIFORM_U11p4",
        "OutputSubdir": "UNIFORM/U11p4",
        "Override.WindModel": "UNIFORM",
        "Override.TurbModel": "IEC_KAIMAL",
        **base("Sanity", "Uniform mean wind only", 11.4),
    })

    return headers, rows


def fill_readme(ws, title: str, structure: str, total_cases: int) -> None:
    lines = [
        title,
        "",
        "Template basis: NREL 5MW reference turbine",
        f"  Hub height: {NREL_5MW['HubHeight_m']} m",
        f"  Rotor diameter: {NREL_5MW['RotorDiameter_m']} m",
        f"  IEC class: {NREL_5MW['IECClass']}",
        f"  Turbulence class: {NREL_5MW['TurbulenceClass']}",
        f"  Cut-in / rated / cut-out: {NREL_5MW['CutIn_mps']} / {NREL_5MW['Rated_mps']} / {NREL_5MW['CutOut_mps']} m/s",
        "",
        "Batch usage:",
        "  1. Open Qahse_WindL_Batch_Template.qwd or your own Mode=BATCH template.",
        "  2. Point BatchExcel to this workbook and BatchSheet to Cases.",
        "  3. Rows use Override.* for WindLInput overrides and Meta.* for record-only offshore metadata.",
        "  4. Current WindL offshore scope is wind-field generation only; Meta.* wave/current/platform fields do not enter SimWind physics.",
        "",
        f"Structure type: {structure}",
        f"Case count: {total_cases}",
    ]
    for row_index, text in enumerate(lines, start=1):
        ws.cell(row_index, 1, text)
    ws.column_dimensions["A"].width = 110


def fill_catalog(ws, headers: Iterable[str]) -> None:
    rows = [
        ("Column", "Meaning"),
        ("CaseName", "Unique batch case name. Also used as default output file stem."),
        ("Enabled", "true/false switch for whether the row runs."),
        ("OutputSubdir", "Case output folder relative to BatchOutputDir."),
        ("Override.*", "Field override applied to WindLInput before generation."),
        ("Meta.*", "Metadata only; recorded in Excel, not written into SimWind physics."),
        ("Override.WindModel", "NTM / ETM / EWM1 / EWM50 / EOG / EDC / ECD / EWS / UNIFORM"),
        ("Override.TurbModel", "IEC_KAIMAL / IEC_VKAIMAL / B_KAL / B_VKAL / B_IVKAL / B_MANN"),
        ("Override.EWMType", "Steady / Turbulent for EWM cases"),
        ("Override.EventSign", "POSITIVE / NEGATIVE for EDC/ECD/EWS event polarity"),
        ("Override.WrTrbts / Override.WrBlwnd / Override.WrTrwnd", "Select output file formats from the same generated wind field."),
    ]
    for r, (left, right) in enumerate(rows, start=1):
        ws.cell(r, 1, left)
        ws.cell(r, 2, right)
        if r == 1:
            ws.cell(r, 1).font = Font(bold=True)
            ws.cell(r, 2).font = Font(bold=True)
    ws.cell(len(rows) + 2, 1, "Workbook columns")
    ws.cell(len(rows) + 2, 1).font = Font(bold=True)
    for idx, header in enumerate(headers, start=len(rows) + 3):
        ws.cell(idx, 1, header)
    ws.column_dimensions["A"].width = 42
    ws.column_dimensions["B"].width = 92


def write_workbook(path: Path, title: str, structure: str, headers: list[str], rows: list[list[str]]) -> None:
    wb = Workbook()
    ws_readme = wb.active
    ws_readme.title = "README"
    fill_readme(ws_readme, title, structure, len(rows))

    ws_cases = wb.create_sheet("Cases")
    with_sheet_header(ws_cases, headers)
    for row_index, row in enumerate(rows, start=2):
        for col_index, value in enumerate(row, start=1):
            ws_cases.cell(row_index, col_index, value)

    ws_catalog = wb.create_sheet("Catalog")
    fill_catalog(ws_catalog, headers)

    path.parent.mkdir(parents=True, exist_ok=True)
    wb.save(path)


def main() -> None:
    fixed_headers, fixed_rows = build_iec_rows("fixed-bottom")
    floating_headers, floating_rows = build_iec_rows("floating")
    generic_headers, generic_rows = build_generic_rows()

    write_workbook(
        OUT_DIR / "IEC61400-3-1_WindCases.xlsx",
        "IEC 61400-3-1 fixed-bottom batch template",
        "fixed-bottom",
        fixed_headers,
        fixed_rows,
    )
    write_workbook(
        OUT_DIR / "IEC61400-3-2_WindCases.xlsx",
        "IEC 61400-3-2 floating batch template",
        "floating",
        floating_headers,
        floating_rows,
    )
    write_workbook(
        OUT_DIR / "Offshore_WindCases_Generic.xlsx",
        "Generic offshore batch template",
        "generic offshore",
        generic_headers,
        generic_rows,
    )

    print("Generated batch templates:")
    print(f"  IEC61400-3-1_WindCases.xlsx : {len(fixed_rows)} cases")
    print(f"  IEC61400-3-2_WindCases.xlsx : {len(floating_rows)} cases")
    print(f"  Offshore_WindCases_Generic.xlsx : {len(generic_rows)} cases")


if __name__ == "__main__":
    main()
