-- Creature spawns and vehicle ids from MaNGOS Three's 4.3.4 world database (https://github.com/mangosthree/database,
-- commit 2e72872), based upon the work of getmangos.eu. Copyright the MaNGOS authors, licensed under the
-- Creative Commons Attribution-NonCommercial-ShareAlike 3.0 license
-- (https://creativecommons.org/licenses/by-nc-sa/3.0/): this file, and any copy or change of it, may not be used
-- commercially and must keep this license. It is not covered by MaelstromCore's GNU GPL v2.
-- Guids are 12,000,000 + the MaNGOS guid; build_world_db.py's mangosspawns stage chose them (turn-in NPCs TDB 434
-- has no spawn for, near their quest's turn-in point).


-- Vehicle ids and speeds of the Goblin start's vehicles (Kezan and the Lost Isles; MaNGOS Three's VehicleTemplateId, SpeedWalk, SpeedRun), which TDB 434 lacks or has at the default
UPDATE `creature_template` SET `VehicleId` = 448 WHERE `entry` = 34840;
UPDATE `creature_template` SET `VehicleId` = 476 WHERE `entry` = 35486;
UPDATE `creature_template` SET `VehicleId` = 582 WHERE `entry` = 37179;
UPDATE `creature_template` SET `VehicleId` = 579 WHERE `entry` = 37213;
UPDATE `creature_template` SET `VehicleId` = 590 WHERE `entry` = 37598;
UPDATE `creature_template` SET `VehicleId` = 592 WHERE `entry` = 37676;
UPDATE `creature_template` SET `VehicleId` = 505 WHERE `entry` = 36143;
UPDATE `creature_template` SET `VehicleId` = 508 WHERE `entry` = 36178;
UPDATE `creature_template` SET `VehicleId` = 524 WHERE `entry` = 36505;
UPDATE `creature_template` SET `VehicleId` = 530 WHERE `entry` = 36585;
UPDATE `creature_template` SET `VehicleId` = 657 WHERE `entry` = 38918;
UPDATE `creature_template` SET `VehicleId` = 530 WHERE `entry` = 39152;
UPDATE `creature_template` SET `VehicleId` = 688 WHERE `entry` = 39329;
UPDATE `creature_template` SET `VehicleId` = 696 WHERE `entry` = 39598;
UPDATE `creature_template` SET `VehicleId` = 628 WHERE `entry` = 38318;
UPDATE `creature_template` SET `VehicleId` = 669 WHERE `entry` = 39074;
UPDATE `creature_template` SET `VehicleId` = 653 WHERE `entry` = 38802;
UPDATE `creature_template` SET `speed_walk` = 0.8, `speed_run` = 2.0 WHERE `entry` = 34840;
