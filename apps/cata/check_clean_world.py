#!/usr/bin/env python3
"""Validate a built clean world against the trusted golden schema (#176).

The client harness imports a world dump, so a stale or incomplete clean dump can
reach the client even when every source build and unit test passes. This checker
covers the content transforms that have caused real regressions: missing Cata
columns/tables, empty creature base stats, stripped script bindings, and the
SmartAI escort-action speed normalization.
"""

import argparse
import json
import re
import subprocess
import sys


def query(container: str, user: str, password: str, schema: str, sql: str) -> list[list[str]]:
    result = subprocess.run(
        ["docker", "exec", container, "mysql", f"-u{user}", f"-p{password}", "-N", "-B", schema, "-e", sql],
        capture_output=True, text=True, check=False,
    )
    if result.returncode:
        raise RuntimeError(f"query failed: {sql}\n{result.stderr.strip()}")
    return [line.split("\t") for line in result.stdout.splitlines()]


def scalar(container, user, password, schema, sql) -> int:
    rows = query(container, user, password, schema, sql)
    return int(rows[0][0]) if rows and rows[0] and rows[0][0] not in ("", "NULL") else 0


def columns(container, user, password, schema, table) -> set[str]:
    rows = query(container, user, password, schema,
                 "SELECT COLUMN_NAME FROM information_schema.COLUMNS "
                 f"WHERE TABLE_SCHEMA='{schema}' AND TABLE_NAME='{table}'")
    return {row[0] for row in rows if row}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--container", default="plan175-inventory-mysql")
    parser.add_argument("--user", default="root")
    parser.add_argument("--password", default="plan175ro")
    parser.add_argument("--clean", required=True, help="built clean world schema")
    parser.add_argument("--reference", default="acore_world_gold", help="trusted golden world schema")
    args = parser.parse_args()
    for name in (args.clean, args.reference):
        if not re.fullmatch(r"\w+", name):
            parser.error(f"invalid schema name: {name!r}")

    problems: list[str] = []
    report: dict[str, object] = {"clean": args.clean, "reference": args.reference, "problems": problems}

    required_columns = {
        ("creature", "PhaseGroup"),
        ("gameobject", "PhaseGroup"),
        ("phase_area", "PhaseGroup"),
        ("creature_classlevelstats", "basehp3"),
        ("creature_classlevelstats", "damage_exp3"),
    }
    for table, column in sorted(required_columns):
        if column not in columns(args.container, args.user, args.password, args.clean, table):
            problems.append(f"missing column {table}.{column}")

    counts = {}
    for table in ("creature", "gameobject", "creature_template", "creature_classlevelstats"):
        clean = scalar(args.container, args.user, args.password, args.clean, f"SELECT COUNT(*) FROM `{table}`")
        reference = scalar(args.container, args.user, args.password, args.reference, f"SELECT COUNT(*) FROM `{table}`")
        counts[table] = {"clean": clean, "reference": reference}
        if clean == 0:
            problems.append(f"{table} is empty")
        if table in ("creature", "gameobject", "creature_classlevelstats") and clean != reference:
            problems.append(f"{table} count differs: clean={clean}, reference={reference}")
    report["counts"] = counts

    missing_templates = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM "
        f"`{args.reference}`.creature_template g LEFT JOIN creature_template c ON c.entry=g.entry "
        "WHERE c.entry IS NULL",
    )
    report["missing_creature_templates"] = missing_templates
    if missing_templates:
        problems.append(f"{missing_templates} creature templates from {args.reference} are missing")

    script_mismatches = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM "
        f"`{args.reference}`.creature_template g LEFT JOIN creature_template c ON c.entry=g.entry "
        "WHERE c.entry IS NULL OR c.ScriptName<>g.ScriptName",
    )
    report["script_binding_mismatches"] = script_mismatches
    if script_mismatches:
        problems.append(f"{script_mismatches} ScriptName bindings differ from {args.reference}")

    escort = {}
    for param in (1, 2):
        clean = scalar(args.container, args.user, args.password, args.clean,
                       f"SELECT COUNT(*) FROM smart_scripts WHERE action_type=53 AND action_param1={param}")
        reference = scalar(args.container, args.user, args.password, args.reference,
                           f"SELECT COUNT(*) FROM smart_scripts WHERE action_type=53 AND action_param1={param}")
        escort[str(param)] = {"clean": clean, "reference": reference}
        if clean != reference:
            problems.append(f"SmartAI escort action_param1={param} differs: clean={clean}, reference={reference}")
    report["escort_action_counts"] = escort

    for table in ("phase_area", "terrain_swap_defaults", "terrain_worldmap"):
        count = scalar(args.container, args.user, args.password, args.clean, f"SELECT COUNT(*) FROM `{table}`")
        report[table] = count
        if count == 0:
            problems.append(f"{table} is empty")

    legacy_conditions = scalar(args.container, args.user, args.password, args.clean,
                               "SELECT COUNT(*) FROM `conditions` WHERE ConditionTypeOrReference=26")
    report["legacy_phase_conditions"] = legacy_conditions
    if legacy_conditions:
        problems.append(f"{legacy_conditions} legacy phase-mask conditions remain")

    # 59073 is the Gilneas phase-170 aura. Autocast is valid to disable for other
    # spell_area rows, but this phase must be applied automatically in every area.
    phase_autocast_mismatches = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM `spell_area` WHERE `spell`=59073 AND `autocast`<>1",
    )
    report["gilneas_phase_autocast_mismatches"] = phase_autocast_mismatches
    if phase_autocast_mismatches:
        problems.append(
            f"{phase_autocast_mismatches} Gilneas phase 59073 rows are not autocast")

    for spell, start, end in ((68481, 14375, 14321), (68482, 14321, 14386), (68483, 14386, 14466)):
        count = scalar(
            args.container, args.user, args.password, args.clean,
            f"SELECT COUNT(*) FROM spell_area WHERE spell={spell} AND area=4714 "
            f"AND quest_start={start} AND quest_end={end} "
            "AND quest_start_status=64 AND quest_end_status=43 AND autocast=1")
        report[f"gilneas_phase_aura_{spell}"] = count
        if count != 1:
            problems.append(f"missing automatic Gilneas phase aura {spell} with its native quest window")

    for spell, entry in ((68558, 36140), (68591, 36286), (68682, 36312), (69123, 36198)):
        count = scalar(
            args.container, args.user, args.password, args.clean,
            "SELECT COUNT(*) FROM conditions WHERE SourceTypeOrReferenceId=13 AND SourceGroup=1 "
            f"AND SourceEntry={spell} AND ConditionTypeOrReference=31 "
            f"AND ConditionValue1=3 AND ConditionValue2={entry} AND NegativeCondition=0")
        report[f"gilneas_target_{spell}"] = count
        if count != 1:
            problems.append(f"missing Gilneas spell {spell} target restriction to creature {entry}")

    destinations = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM spell_target_position WHERE ID IN (68630,72788) AND EffectIndex=0 "
        "AND MapID=654 AND ABS(PositionX+1818.4)<0.01 AND ABS(PositionY-2294.25)<0.01 "
        "AND ABS(PositionZ-42.2135)<0.01 AND ABS(Orientation-3.24666)<0.01")
    report["gilneas_transition_destinations"] = destinations
    if destinations != 2:
        problems.append("missing Gilneas transition destinations for Curse of the Worgen / Last Stand Complete")

    bindings = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM spell_script_names WHERE "
        "(spell_id=68996 AND ScriptName='spell_gen_two_forms') OR "
        "(spell_id=68992 AND ScriptName='spell_gen_darkflight')")
    report["worgen_form_bindings"] = bindings
    if bindings != 2:
        problems.append("missing Two Forms / Darkflight spell script bindings")

    item_template_entries = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM item_template")
    report["item_template_entries"] = item_template_entries
    if item_template_entries != 64775:
        problems.append(f"expected 64775 item_template entries, found {item_template_entries}")

    missing_equipment_items = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(DISTINCT item) FROM ("
        "SELECT ItemID1 item FROM creature_equip_template WHERE ItemID1<>0 "
        "UNION SELECT ItemID2 FROM creature_equip_template WHERE ItemID2<>0 "
        "UNION SELECT ItemID3 FROM creature_equip_template WHERE ItemID3<>0) e "
        "LEFT JOIN item_template i ON i.entry=e.item WHERE i.entry IS NULL")
    report["missing_equipment_items"] = missing_equipment_items
    if missing_equipment_items:
        problems.append(f"{missing_equipment_items} equipment item ids have no item_template row")

    gilneas_phase_areas = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM phase_area WHERE PhaseId IN (170,171) AND AreaId IN (4755,4756,4757)")
    report["gilneas_phase_area_rows"] = gilneas_phase_areas
    if gilneas_phase_areas != 6:
        problems.append(f"expected 6 Gilneas phase-170/171 phase_area rows, found {gilneas_phase_areas}")

    gilneas_phase_172_rows = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM phase_area WHERE PhaseId=172 AND AreaId IN (4757,4758)")
    report["gilneas_phase_172_rows"] = gilneas_phase_172_rows
    if gilneas_phase_172_rows != 2:
        problems.append(f"expected 2 Gilneas phase-172 rows, found {gilneas_phase_172_rows}")

    gilneas_phase_condition = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM conditions WHERE SourceTypeOrReferenceId=26 "
        "AND SourceGroup IN (170,171,172) AND ConditionValue1 IN (14078,14159,14294,14293)")
    report["gilneas_phase_condition"] = gilneas_phase_condition
    if gilneas_phase_condition != 5:
        problems.append(f"expected five Gilneas phase 170/171/172 conditions, found {gilneas_phase_condition}")

    # A class trainer with several GOSSIP_OPTION_TRAINER rows shows duplicate
    # training entries. Profession trainers legitimately have more than one.
    duplicate_trainer_options = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM ("
        "SELECT g.MenuID FROM gossip_menu_option g "
        "JOIN creature_template c ON c.gossip_menu_id=g.MenuID "
        "JOIN creature_default_trainer cdt ON cdt.CreatureId=c.entry "
        "JOIN trainer t ON t.Id=cdt.TrainerId "
        "WHERE g.OptionType=5 AND t.Type=0 "
        "GROUP BY g.MenuID HAVING COUNT(DISTINCT g.OptionID)>1"
        ") AS duplicates",
    )
    report["duplicate_class_trainer_options"] = duplicate_trainer_options
    if duplicate_trainer_options:
        problems.append(
            f"{duplicate_trainer_options} class-trainer menus have duplicate training options")

    # 19696 is Lorna's mastiff voice-over; only her mastiff line (38744) may use it.
    # TDB applies it to Crowley, Liam, and Genn lines, which is audible during Sacrifices.
    mastiff_sound_misuse = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM creature_text WHERE Sound=19696 AND BroadcastTextId<>38744")
    report["mastiff_sound_misuse"] = mastiff_sound_misuse
    if mastiff_sound_misuse:
        problems.append(f"{mastiff_sound_misuse} non-Lorna rows use Lorna's mastiff sound")
    # TDB 434 ships 685 spellclick, 422 vehicle-accessory and 15 seat-addon rows. The
    # starter-zone import only reaches spawned creatures, so summoned vehicles such as
    # Crowley's Horse lose their click/accessory rows unless the template phase copies them.
    vehicle_tables = {
        table: scalar(args.container, args.user, args.password, args.clean, f"SELECT COUNT(*) FROM `{table}`")
        for table in ("npc_spellclick_spells", "vehicle_template_accessory", "vehicle_seat_addon")
    }
    report["vehicle_tables"] = vehicle_tables
    if vehicle_tables["npc_spellclick_spells"] < 600:
        problems.append(f"npc_spellclick_spells is incomplete: {vehicle_tables['npc_spellclick_spells']} rows")
    if vehicle_tables["vehicle_template_accessory"] < 400:
        problems.append(
            f"vehicle_template_accessory is incomplete: {vehicle_tables['vehicle_template_accessory']} rows")
    if not vehicle_tables["vehicle_seat_addon"]:
        problems.append("vehicle_seat_addon is empty")
    horse_spellclick = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM npc_spellclick_spells WHERE npc_entry IN (35231,44428) AND spell_id=46598")
    horse_accessory = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM vehicle_template_accessory WHERE entry=35231 AND accessory_entry=35230")
    report["crowley_horse_spellclick"] = horse_spellclick
    report["crowley_horse_accessory"] = horse_accessory
    if horse_spellclick != 2:
        problems.append(f"expected 2 Crowley's Horse spellclick rows, found {horse_spellclick}")
    if horse_accessory != 1:
        problems.append(f"expected Crowley as Crowley's Horse accessory, found {horse_accessory}")

    # Spellclick conditions are separate source-type-18 rows; an empty list means
    # unrestricted, so a restore that drops them silently opens quest-gated vehicles.
    spellclick_conditions = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM conditions WHERE SourceTypeOrReferenceId=18")
    report["spellclick_conditions"] = spellclick_conditions
    if spellclick_conditions < 187:
        problems.append(f"spellclick conditions are incomplete: {spellclick_conditions} rows")
    spellclick_example = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM conditions WHERE SourceTypeOrReferenceId=18 "
        "AND SourceGroup=25334 AND SourceEntry=46598 AND ConditionTypeOrReference=9 "
        "AND ConditionValue1=11652")
    report["spellclick_example_condition"] = spellclick_example
    if spellclick_example != 1:
        problems.append(f"expected the NPC 25334 spellclick quest condition, found {spellclick_example}")

    graveyard_links = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM graveyard_zone")
    report["graveyard_links"] = graveyard_links
    if not graveyard_links:
        problems.append("graveyard_zone is empty")

    unresolved_graveyard_links = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM graveyard_zone gz "
        "LEFT JOIN game_graveyard gg ON gg.ID=gz.ID WHERE gg.ID IS NULL")
    report["unresolved_graveyard_links"] = unresolved_graveyard_links
    if unresolved_graveyard_links:
        problems.append(
            f"{unresolved_graveyard_links} graveyard-zone links have no game_graveyard location")

    gilneas_graveyard_links = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM graveyard_zone gz "
        "JOIN game_graveyard gg ON gg.ID=gz.ID "
        "WHERE gz.GhostZone=4755 AND gg.Map=654")
    report["gilneas_graveyard_links"] = gilneas_graveyard_links
    if not gilneas_graveyard_links:
        problems.append("Gilneas has no linked map-654 graveyard")

    low_level_stats = scalar(args.container, args.user, args.password, args.clean,
                             "SELECT COUNT(*) FROM creature_classlevelstats "
                             "WHERE level<=5 AND (basehp0<=1 OR damage_base<=0)")
    report["bad_low_level_stats"] = low_level_stats
    if low_level_stats:
        problems.append(f"{low_level_stats} low-level class stats have 1 HP or zero damage")

    # Waypoint geometry. A smooth (catmullrom) waypoint spline runs through a path's nodes up to the next node with
    # a delay; waypoint_data_addon points form one spline to their node. MoveSplineInitArgs::_checkPathLengths
    # rejects such a spline when two consecutive points are closer than Movement::MIN_SPLINE_SEGMENT_LENGTH, and the
    # creature never moves. The world server merges these at load and logs them; the built world must not have them.
    min_segment = 0.1
    dist = ("SQRT(POW({a}.position_x-{b}.position_x,2)+POW({a}.position_y-{b}.position_y,2)"
            "+POW({a}.position_z-{b}.position_z,2))")
    smooth_duplicates = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM waypoint_data a JOIN waypoint_data b ON b.id=a.id AND b.point=("
        "SELECT MIN(n.point) FROM waypoint_data n WHERE n.id=a.id AND n.point>a.point) "
        "WHERE a.delay=0 AND a.id IN (SELECT id FROM waypoint_data WHERE smoothTransition=1) "
        f"AND {dist.format(a='a', b='b')}<{min_segment}")
    report["waypoint_smooth_duplicate_points"] = smooth_duplicates
    if smooth_duplicates:
        problems.append(f"{smooth_duplicates} waypoint_data points repeat the previous point inside a smooth path")

    # A repeating path is closed by the movement generator itself; a last node on top of the first breaks the spline.
    closed_loops = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM (SELECT id, MIN(point) mn, MAX(point) mx FROM waypoint_data GROUP BY id "
        "HAVING COUNT(*)>2) m JOIN waypoint_data f ON f.id=m.id AND f.point=m.mn "
        "JOIN waypoint_data l ON l.id=m.id AND l.point=m.mx "
        "WHERE l.delay=0 AND m.id IN (SELECT id FROM waypoint_data WHERE smoothTransition=1) "
        "AND m.id IN (SELECT ca.path_id FROM creature_addon ca JOIN creature c ON c.guid=ca.guid "
        "WHERE c.MovementType=2 UNION SELECT cta.path_id FROM creature_template_addon cta "
        "JOIN creature c ON c.id=cta.entry WHERE c.MovementType=2) "
        f"AND {dist.format(a='f', b='l')}<{min_segment}")
    report["waypoint_closed_repeating_loops"] = closed_loops
    if closed_loops:
        problems.append(f"{closed_loops} repeating smooth waypoint paths end on their first point")

    addon_dist = ("SQRT(POW(a.PositionX-b.PositionX,2)+POW(a.PositionY-b.PositionY,2)"
                  "+POW(a.PositionZ-b.PositionZ,2))")
    addon_duplicates = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM waypoint_data_addon a JOIN waypoint_data_addon b ON b.PathID=a.PathID "
        "AND b.PointID=a.PointID AND b.SplinePointIndex=a.SplinePointIndex+1 "
        f"WHERE {addon_dist}<{min_segment}")
    addon_on_node = scalar(
        args.container, args.user, args.password, args.clean,
        "SELECT COUNT(*) FROM waypoint_data_addon a JOIN (SELECT PathID, PointID, MAX(SplinePointIndex) mx "
        "FROM waypoint_data_addon GROUP BY PathID, PointID) m ON m.PathID=a.PathID AND m.PointID=a.PointID "
        "AND a.SplinePointIndex=m.mx JOIN waypoint_data w ON w.id=a.PathID AND w.point=a.PointID "
        "WHERE SQRT(POW(a.PositionX-w.position_x,2)+POW(a.PositionY-w.position_y,2)"
        f"+POW(a.PositionZ-w.position_z,2))<{min_segment}")
    report["waypoint_addon_duplicate_points"] = addon_duplicates + addon_on_node
    if addon_duplicates + addon_on_node:
        problems.append(f"{addon_duplicates + addon_on_node} waypoint_data_addon spline points repeat "
                        "the previous point or their waypoint")

    print(json.dumps(report, indent=2, sort_keys=True))
    if problems:
        print("clean world FAIL", file=sys.stderr)
        return 1
    print("clean world PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, ValueError) as error:
        print(f"clean world check failed: {error}", file=sys.stderr)
        sys.exit(1)
