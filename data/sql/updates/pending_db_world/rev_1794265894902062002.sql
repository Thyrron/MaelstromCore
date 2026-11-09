-- Degenerate waypoint geometry: consecutive points under 0.1 yd apart (Movement::MIN_SPLINE_SEGMENT_LENGTH) inside
-- a smooth (catmullrom) spline make MoveSplineInitArgs::_checkPathLengths reject the whole spline, so the creature
-- never moves and the waypoint generator retried it every update (one Server.log line per tick).
-- Only rows that can end up inside such a spline are removed: the earlier point of each pair has no delay (a delay
-- ends the spline), and none of the removed rows carries a delay, orientation, script or waypoint_data_addon points.
-- Intentional "stop, then turn and wait" pairs (paths 3500870, 3543750) are kept.

-- Repeated point right after a point without delay, in paths with smooth nodes
DELETE FROM `waypoint_data` WHERE `id` = 491230 AND `point` = 16;
DELETE FROM `waypoint_data` WHERE `id` = 1324010 AND `point` = 7;
DELETE FROM `waypoint_data` WHERE `id` = 1324030 AND `point` = 11;
DELETE FROM `waypoint_data` WHERE `id` = 1324100 AND `point` = 6;
DELETE FROM `waypoint_data` WHERE `id` = 1324150 AND `point` = 17;
DELETE FROM `waypoint_data` WHERE `id` = 1383120 AND `point` = 20;
DELETE FROM `waypoint_data` WHERE `id` = 1383140 AND `point` = 34;
DELETE FROM `waypoint_data` WHERE `id` = 1383210 AND `point` = 35;
DELETE FROM `waypoint_data` WHERE `id` = 1383270 AND `point` = 18;
DELETE FROM `waypoint_data` WHERE `id` = 1383410 AND `point` = 41;
DELETE FROM `waypoint_data` WHERE `id` = 1383480 AND `point` = 26;
DELETE FROM `waypoint_data` WHERE `id` = 1383520 AND `point` = 37;
DELETE FROM `waypoint_data` WHERE `id` = 2087870 AND `point` = 14;
DELETE FROM `waypoint_data` WHERE `id` = 2087880 AND `point` = 8;
-- points 49 to 57 of this path are one spot repeated nine times
DELETE FROM `waypoint_data` WHERE `id` = 2749170 AND `point` BETWEEN 50 AND 57;

-- Repeating loops whose last point repeats their first: the generator already closes the loop back to the first point
DELETE FROM `waypoint_data` WHERE `id` = 249430 AND `point` = 17;
DELETE FROM `waypoint_data` WHERE `id` = 1158350 AND `point` = 17;
DELETE FROM `waypoint_data` WHERE `id` = 1158370 AND `point` = 12;
DELETE FROM `waypoint_data` WHERE `id` = 1158430 AND `point` = 13;
DELETE FROM `waypoint_data` WHERE `id` = 1158440 AND `point` = 11;
DELETE FROM `waypoint_data` WHERE `id` = 1158450 AND `point` = 13;

-- Spline points 1 to 3 of this waypoint repeat point 0
DELETE FROM `waypoint_data_addon` WHERE `PathID` = 4238300 AND `PointID` = 0 AND `SplinePointIndex` BETWEEN 1 AND 3;
