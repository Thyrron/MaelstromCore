/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "WaypointMgr.h"
#include "DatabaseEnv.h"
#include "GridDefines.h"
#include "Log.h"
#include "MoveSplineInitArgs.h"
#include "QueryResult.h"
#include "Timer.h"
#include <algorithm>

namespace
{
    // Smooth waypoint movement launches the nodes of a path up to the next node with a delay as one catmullrom spline,
    // and waypoint_data_addon spline points are launched as one spline to their node; MoveSplineInitArgs::
    // _checkPathLengths rejects either when two consecutive points are closer than Movement::MIN_SPLINE_SEGMENT_LENGTH.
    // Such data is merged here, at load, so a stored path cannot produce that spline. A node with a delay ends its
    // spline, so a node repeated after it ("stop, then turn and wait") is valid and kept; so are paths without smooth
    // nodes, which move point to point.
    void MergeDuplicateNodes(WaypointPath& path)
    {
        if (std::none_of(path.Nodes.begin(), path.Nodes.end(), [](WaypointNode const& node) { return node.SmoothTransition; }))
            return;

        std::vector<WaypointNode> nodes;
        nodes.reserve(path.Nodes.size());
        for (WaypointNode& node : path.Nodes)
        {
            if (!nodes.empty())
            {
                WaypointNode& kept = nodes.back();
                if (!kept.Delay && (G3D::Vector3(node.X, node.Y, node.Z) - G3D::Vector3(kept.X, kept.Y, kept.Z)).length() < Movement::MIN_SPLINE_SEGMENT_LENGTH)
                {
                    LOG_ERROR("sql.sql", "Waypoint path {}: point {} is closer than {} yd to point {} and was merged into it. Remove it from `waypoint_data`.",
                        path.Id, node.Id, Movement::MIN_SPLINE_SEGMENT_LENGTH, kept.Id);
                    // the kept node has no delay, so the merged one takes over what happens at this spot
                    kept.Delay = node.Delay;
                    if (node.Orientation)
                        kept.Orientation = node.Orientation;
                    if (node.EventId)
                    {
                        if (kept.EventId)
                            LOG_ERROR("sql.sql", "Waypoint path {}: script {} of merged point {} replaces script {} of point {}.",
                                path.Id, node.EventId, node.Id, kept.EventId, kept.Id);
                        kept.EventId = node.EventId;
                        kept.EventChance = node.EventChance;
                    }
                    continue;
                }
            }

            nodes.push_back(std::move(node));
        }

        path.Nodes = std::move(nodes);
    }

    void RemoveDuplicateSplinePoints(uint32 pathId, WaypointNode& node)
    {
        std::vector<G3D::Vector3> points;
        points.reserve(node.SplinePoints.size());
        for (G3D::Vector3 const& point : node.SplinePoints)
            if (points.empty() || (point - points.back()).length() >= Movement::MIN_SPLINE_SEGMENT_LENGTH)
                points.push_back(point);

        // the spline ends on the node itself
        G3D::Vector3 const destination(node.X, node.Y, node.Z);
        while (!points.empty() && (destination - points.back()).length() < Movement::MIN_SPLINE_SEGMENT_LENGTH)
            points.pop_back();

        if (points.size() == node.SplinePoints.size())
            return;

        LOG_ERROR("sql.sql", "Waypoint path {} point {}: removed {} spline point(s) closer than {} yd to the previous point or to the waypoint. Remove them from `waypoint_data_addon`.",
            pathId, node.Id, node.SplinePoints.size() - points.size(), Movement::MIN_SPLINE_SEGMENT_LENGTH);
        node.SplinePoints = std::move(points);
    }
}

WaypointMgr* WaypointMgr::instance()
{
    static WaypointMgr instance;
    return &instance;
}

void WaypointMgr::Load()
{
    uint32 oldMSTime = getMSTime();

    //                                                0    1         2           3          4            5          6      7      8                 9         10       11
    QueryResult result = WorldDatabase.Query("SELECT id, point, position_x, position_y, position_z, orientation, velocity, delay, smoothTransition, move_type, action, action_chance FROM waypoint_data ORDER BY id, point");

    if (!result)
    {
        LOG_WARN("server.loading", ">> Loaded 0 waypoints. DB table `waypoint_data` is empty!");
        LOG_INFO("server.loading", " ");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();
        uint32 pathId = fields[0].Get<uint32>();
        float x = fields[2].Get<float>();
        float y = fields[3].Get<float>();
        float z = fields[4].Get<float>();
        std::optional<float> o;
        if (!fields[5].IsNull())
            o = fields[5].Get<float>();

        float velocity = fields[6].Get<float>();

        Acore::NormalizeMapCoord(x);
        Acore::NormalizeMapCoord(y);

        WaypointNode waypoint;
        waypoint.Id = fields[1].Get<uint32>();
        waypoint.X = x;
        waypoint.Y = y;
        waypoint.Z = z;
        if (o.has_value())
            waypoint.Orientation = o;
        waypoint.Velocity = velocity;
        waypoint.Delay = fields[7].Get<uint32>();
        waypoint.SmoothTransition = fields[8].Get<bool>();
        waypoint.MoveType = fields[9].Get<uint32>();

        if (waypoint.MoveType >= WAYPOINT_MOVE_TYPE_MAX)
        {
            LOG_ERROR("sql.sql", "Waypoint {} in waypoint_data has invalid move_type, ignoring", waypoint.Id);
            continue;
        }

        waypoint.EventId = fields[10].Get<uint32>();
        waypoint.EventChance = fields[11].Get<int16>();

        WaypointPath& path = _waypointStore[pathId];
        path.Id = pathId;
        path.Nodes.push_back(std::move(waypoint));
        ++count;
    } while (result->NextRow());

    for (auto& [pathId, path] : _waypointStore)
        MergeDuplicateNodes(path);

    LOG_INFO("server.loading", ">> Loaded {} waypoints in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
    LOG_INFO("server.loading", " ");
}

void WaypointMgr::LoadWaypointAddons()
{
    uint32 oldMSTime = getMSTime();

    //                                               0       1        2                 3          4          5
    QueryResult result = WorldDatabase.Query("SELECT PathID, PointID, SplinePointIndex, PositionX, PositionY, PositionZ FROM waypoint_data_addon ORDER BY PathID, PointID, SplinePointIndex");

    if (!result)
    {
        LOG_INFO("server.loading", ">> Loaded 0 waypoint addon data. DB table `waypoint_data_addon` is empty!");
        LOG_INFO("server.loading", " ");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();
        uint32 pathId = fields[0].Get<uint32>();

        auto it = _waypointStore.find(pathId);
        if (it == _waypointStore.end())
        {
            LOG_ERROR("sql.sql", "Tried to load waypoint_data_addon data for PathID {} but there is no such path in waypoint_data. Ignoring.", pathId);
            continue;
        }

        WaypointPath& path = it->second;
        uint32 pointId = fields[1].Get<uint32>();

        auto itr = std::find_if(path.Nodes.begin(), path.Nodes.end(), [pointId](WaypointNode const& node)
        {
            return node.Id == pointId;
        });

        if (itr == path.Nodes.end())
        {
            LOG_ERROR("sql.sql", "Tried to load waypoint_data_addon data for PointID {} of PathID {} but there is no such point in waypoint_data. Ignoring.", pointId, pathId);
            continue;
        }

        float x = fields[3].Get<float>();
        float y = fields[4].Get<float>();
        float z = fields[5].Get<float>();

        Acore::NormalizeMapCoord(x);
        Acore::NormalizeMapCoord(y);

        itr->SplinePoints.push_back(G3D::Vector3(x, y, z));

        ++count;
    } while (result->NextRow());

    for (auto& [pathId, path] : _waypointStore)
        for (WaypointNode& node : path.Nodes)
            if (!node.SplinePoints.empty())
                RemoveDuplicateSplinePoints(pathId, node);

    LOG_INFO("server.loading", ">> Loaded {} waypoint addon data in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
    LOG_INFO("server.loading", " ");
}

void WaypointMgr::ReloadPath(uint32 id)
{
    auto itr = _waypointStore.find(id);
    if (itr != _waypointStore.end())
        _waypointStore.erase(itr);

    WorldDatabasePreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_WAYPOINT_DATA_BY_ID);

    stmt->SetData(0, id);

    PreparedQueryResult result = WorldDatabase.Query(stmt);

    if (!result)
        return;

    std::vector<WaypointNode> values;
    do
    {
        Field* fields = result->Fetch();
        float x = fields[1].Get<float>();
        float y = fields[2].Get<float>();
        float z = fields[3].Get<float>();
        std::optional<float> o;
        if (!fields[4].IsNull())
            o = fields[4].Get<float>();

        float velocity = fields[5].Get<float>();

        Acore::NormalizeMapCoord(x);
        Acore::NormalizeMapCoord(y);

        WaypointNode waypoint;
        waypoint.Id = fields[0].Get<uint32>();
        waypoint.X = x;
        waypoint.Y = y;
        waypoint.Z = z;
        if (o.has_value())
            waypoint.Orientation = o;
        waypoint.Velocity = velocity;
        waypoint.Delay = fields[6].Get<uint32>();
        waypoint.SmoothTransition = fields[7].Get<bool>();
        waypoint.MoveType = fields[8].Get<uint32>();

        if (waypoint.MoveType >= WAYPOINT_MOVE_TYPE_MAX)
        {
            LOG_ERROR("sql.sql", "Waypoint {} in waypoint_data has invalid move_type, ignoring", waypoint.Id);
            continue;
        }

        waypoint.EventId = fields[9].Get<uint32>();
        waypoint.EventChance = fields[10].Get<uint8>();

        values.push_back(std::move(waypoint));
    } while (result->NextRow());

    WaypointPath& path = _waypointStore[id] = WaypointPath(id, std::move(values));
    MergeDuplicateNodes(path);
}
