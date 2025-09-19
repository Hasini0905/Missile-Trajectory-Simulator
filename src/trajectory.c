#include "trajectory.h"
#include <math.h>

// Convert degrees to radians
static double deg2rad(double degrees) {
    return degrees * M_PI / 180.0;
}

// Convert radians to degrees
static double rad2deg(double radians) {
    return radians * 180.0 / M_PI;
}

// Calculate distance between two coordinates using the Haversine formula
double calculateDistance(Coordinates start, Coordinates end) {
    double lat1 = deg2rad(start.latitude);
    double lon1 = deg2rad(start.longitude);
    double lat2 = deg2rad(end.latitude);
    double lon2 = deg2rad(end.longitude);
    
    double dlon = lon2 - lon1;
    double dlat = lat2 - lat1;
    
    double a = sin(dlat/2) * sin(dlat/2) + cos(lat1) * cos(lat2) * sin(dlon/2) * sin(dlon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    double distance = EARTH_RADIUS * c;
    
    return distance; // in kilometers
}

// Calculate initial bearing from start to end point
double calculateBearing(Coordinates start, Coordinates end) {
    double lat1 = deg2rad(start.latitude);
    double lon1 = deg2rad(start.longitude);
    double lat2 = deg2rad(end.latitude);
    double lon2 = deg2rad(end.longitude);
    
    double dlon = lon2 - lon1;
    
    double y = sin(dlon) * cos(lat2);
    double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dlon);
    double bearing = atan2(y, x);
    
    return fmod(rad2deg(bearing) + 360.0, 360.0); // in degrees
}

// Calculate travel time based on distance and speed
double calculateTravelTime(double distance, double speed) {
    // Convert distance to meters (from km)
    double distanceInMeters = distance * 1000.0;
    
    // Calculate time in seconds
    return distanceInMeters / speed;
}

// Calculate intermediate point along the great circle path
Coordinates calculateIntermediatePoint(Coordinates start, Coordinates end, double fraction) {
    double lat1 = deg2rad(start.latitude);
    double lon1 = deg2rad(start.longitude);
    double lat2 = deg2rad(end.latitude);
    double lon2 = deg2rad(end.longitude);
    
    double d = calculateDistance(start, end) / EARTH_RADIUS; // Angular distance
    
    double a = sin((1-fraction) * d) / sin(d);
    double b = sin(fraction * d) / sin(d);
    
    double x = a * cos(lat1) * cos(lon1) + b * cos(lat2) * cos(lon2);
    double y = a * cos(lat1) * sin(lon1) + b * cos(lat2) * sin(lon2);
    double z = a * sin(lat1) + b * sin(lat2);
    
    double lat = atan2(z, sqrt(x*x + y*y));
    double lon = atan2(y, x);
    
    Coordinates result;
    result.latitude = rad2deg(lat);
    result.longitude = rad2deg(lon);
    
    // Simple altitude calculation (parabolic path)
    double maxAlt = 10000.0; // Maximum altitude in meters
    double altFraction = sin(fraction * M_PI); // Parabolic curve
    result.altitude = maxAlt * altFraction;
    
    return result;
}

// Add a waypoint to the trajectory
void addWaypoint(TrajectoryData* trajectory, Coordinates position, double turnAngle) {
    if (trajectory->waypointCount >= MAX_WAYPOINTS) {
        return; // Maximum waypoints reached
    }
    
    int idx = trajectory->waypointCount;
    trajectory->waypoints[idx].position = position;
    trajectory->waypoints[idx].turnAngle = turnAngle;
    
    // Initialize other values to zero, they will be calculated later
    trajectory->waypoints[idx].approachSpeed = 0.0;
    trajectory->waypoints[idx].departureSpeed = 0.0;
    trajectory->waypoints[idx].timeToReach = 0.0;
    trajectory->waypoints[idx].distanceFromPrevious = 0.0;
    trajectory->waypoints[idx].bearingFromPrevious = 0.0;
    trajectory->waypoints[idx].fuelConsumed = 0.0;
    trajectory->waypoints[idx].gForce = 0.0;
    
    trajectory->waypointCount++;
    
    // Recalculate the full trajectory with the new waypoint
    calculateFullTrajectory(trajectory);
}

// Calculate the effect of a turn on missile speed
double calculateTurnEffect(double speed, double turnAngle, double dragCoefficient) {
    // Simple model: speed reduction proportional to turn angle and drag
    // More realistic models would consider turn radius, missile aerodynamics, etc.
    double turnRadians = fabs(deg2rad(turnAngle));
    double speedReductionFactor = cos(turnRadians * dragCoefficient);
    
    // Ensure we don't get negative speeds
    return fmax(speed * speedReductionFactor, 0.1 * speed);
}

// Calculate G-force during a turn
double calculateGForce(double speed, double turnRadius) {
    // G-force = v²/r/g
    // Convert speed to m/s if it's not already
    double speedInMeters = speed; // Assuming speed is already in m/s
    
    // Prevent division by zero
    if (turnRadius < 0.1) turnRadius = 0.1;
    
    // Calculate G-force (centripetal acceleration / g)
    return (speedInMeters * speedInMeters) / (turnRadius * GRAVITY);
}

// Calculate the effects of a waypoint on the trajectory
void calculateWaypointEffects(TrajectoryData* trajectory, int waypointIndex) {
    if (waypointIndex <= 0 || waypointIndex >= trajectory->waypointCount) {
        return; // Invalid waypoint index
    }
    
    // Get the previous point (could be start or another waypoint)
    Coordinates prevPoint;
    double prevSpeed;
    
    if (waypointIndex == 1) {
        prevPoint = trajectory->start;
        prevSpeed = trajectory->missile.speed; // Initial speed
    } else {
        prevPoint = trajectory->waypoints[waypointIndex - 1].position;
        prevSpeed = trajectory->waypoints[waypointIndex - 1].departureSpeed;
    }
    
    Waypoint* waypoint = &trajectory->waypoints[waypointIndex];
    
    // Calculate distance and bearing from previous point
    waypoint->distanceFromPrevious = calculateDistance(prevPoint, waypoint->position);
    waypoint->bearingFromPrevious = calculateBearing(prevPoint, waypoint->position);
    
    // Set approach speed (same as departure speed from previous point)
    waypoint->approachSpeed = prevSpeed;
    
    // Calculate time to reach this waypoint
    waypoint->timeToReach = calculateTravelTime(waypoint->distanceFromPrevious * 1000, waypoint->approachSpeed);
    
    // Calculate speed after turn
    waypoint->departureSpeed = calculateTurnEffect(
        waypoint->approachSpeed,
        waypoint->turnAngle,
        trajectory->missile.dragCoefficient
    );
    
    // Calculate fuel consumed to reach this waypoint
    // Base fuel consumption during normal flight
    double normalFuelConsumption = waypoint->timeToReach * trajectory->missile.fuelConsumptionNormal;
    
    // Additional fuel for the turn
    double turnTime = fabs(waypoint->turnAngle) / trajectory->missile.maxTurnRate;
    double turnFuelConsumption = turnTime * trajectory->missile.fuelConsumptionTurn;
    
    waypoint->fuelConsumed = normalFuelConsumption + turnFuelConsumption;
    
    // Calculate G-force during turn (simplified)
    // Estimate turn radius based on speed and turn rate
    double turnRadiusMeters = waypoint->approachSpeed / deg2rad(trajectory->missile.maxTurnRate);
    waypoint->gForce = calculateGForce(waypoint->approachSpeed, turnRadiusMeters);
}

// Calculate the full trajectory with all waypoints
void calculateFullTrajectory(TrajectoryData* trajectory) {
    // Reset total values
    trajectory->totalDistance = 0.0;
    trajectory->totalTravelTime = 0.0;
    trajectory->currentSpeed = trajectory->missile.speed;
    trajectory->remainingFuel = trajectory->missile.fuel;
    
    // Calculate initial bearing from start to first point (waypoint or end)
    if (trajectory->waypointCount > 0) {
        trajectory->initialBearing = calculateBearing(trajectory->start, trajectory->waypoints[0].position);
    } else {
        trajectory->initialBearing = calculateBearing(trajectory->start, trajectory->end);
    }
    
    // Calculate effects for each waypoint
    for (int i = 0; i < trajectory->waypointCount; i++) {
        calculateWaypointEffects(trajectory, i);
        
        // Update totals
        trajectory->totalDistance += trajectory->waypoints[i].distanceFromPrevious;
        trajectory->totalTravelTime += trajectory->waypoints[i].timeToReach;
        trajectory->remainingFuel -= trajectory->waypoints[i].fuelConsumed;
        
        // Update current speed
        trajectory->currentSpeed = trajectory->waypoints[i].departureSpeed;
    }
    
    // Calculate final leg (last waypoint to end or start to end if no waypoints)
    Coordinates lastPoint;
    double lastSpeed;
    
    if (trajectory->waypointCount > 0) {
        lastPoint = trajectory->waypoints[trajectory->waypointCount - 1].position;
        lastSpeed = trajectory->waypoints[trajectory->waypointCount - 1].departureSpeed;
    } else {
        lastPoint = trajectory->start;
        lastSpeed = trajectory->missile.speed;
    }
    
    double finalDistance = calculateDistance(lastPoint, trajectory->end);
    double finalTime = calculateTravelTime(finalDistance * 1000, lastSpeed);
    double finalFuel = finalTime * trajectory->missile.fuelConsumptionNormal;
    
    // Update totals with final leg
    trajectory->totalDistance += finalDistance;
    trajectory->totalTravelTime += finalTime;
    trajectory->remainingFuel -= finalFuel;
    
    // Ensure remaining fuel doesn't go negative
    if (trajectory->remainingFuel < 0) {
        trajectory->remainingFuel = 0;
    }
}

// Generate path points for visualization
Coordinates* generatePathPoints(TrajectoryData* trajectory, int* pointCount) {
    // Determine how many segments we have (waypoints + 1)
    int segments = trajectory->waypointCount + 1;
    
    // Allocate memory for path points (100 points per segment)
    int totalPoints = segments * 100;
    Coordinates* pathPoints = (Coordinates*)malloc(totalPoints * sizeof(Coordinates));
    
    if (!pathPoints) {
        *pointCount = 0;
        return NULL;
    }
    
    int currentPoint = 0;
    
    // Generate points for each segment
    for (int segment = 0; segment < segments; segment++) {
        Coordinates segmentStart, segmentEnd;
        
        // Determine start and end of this segment
        if (segment == 0) {
            segmentStart = trajectory->start;
        } else {
            segmentStart = trajectory->waypoints[segment - 1].position;
        }
        
        if (segment == segments - 1) {
            segmentEnd = trajectory->end;
        } else {
            segmentEnd = trajectory->waypoints[segment].position;
        }
        
        // Generate 100 points along this segment
        for (int i = 0; i < 100; i++) {
            double fraction = (double)i / 99.0;
            pathPoints[currentPoint] = calculateIntermediatePoint(segmentStart, segmentEnd, fraction);
            currentPoint++;
        }
    }
    
    *pointCount = currentPoint;
    return pathPoints;
}

// Calculate complete trajectory data
TrajectoryData calculateTrajectory(Coordinates start, Coordinates end, MissileAttributes missile) {
    TrajectoryData trajectory;
    
    trajectory.start = start;
    trajectory.end = end;
    trajectory.missile = missile;
    trajectory.waypointCount = 0;
    
    // Set default physics values if not provided
    if (trajectory.missile.maxAcceleration <= 0) trajectory.missile.maxAcceleration = 30.0;
    if (trajectory.missile.maxDeceleration <= 0) trajectory.missile.maxDeceleration = 50.0;
    if (trajectory.missile.maxTurnRate <= 0) trajectory.missile.maxTurnRate = 20.0;
    if (trajectory.missile.dragCoefficient <= 0) trajectory.missile.dragCoefficient = 0.1;
    if (trajectory.missile.fuelConsumptionNormal <= 0) {
        trajectory.missile.fuelConsumptionNormal = trajectory.missile.burnRate;
    }
    if (trajectory.missile.fuelConsumptionTurn <= 0) {
        trajectory.missile.fuelConsumptionTurn = trajectory.missile.burnRate * 2.0;
    }
    
    // Calculate initial trajectory (no waypoints)
    calculateFullTrajectory(&trajectory);
    
    return trajectory;
}
