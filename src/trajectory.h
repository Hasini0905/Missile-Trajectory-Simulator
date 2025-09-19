#ifndef TRAJECTORY_H
#define TRAJECTORY_H

#include <math.h>
#include <stdlib.h>

// Earth radius in kilometers
#define EARTH_RADIUS 6371.0

// Constants for physics calculations
#define GRAVITY 9.81 // m/s^2
#define MAX_WAYPOINTS 10 // Maximum number of waypoints

// Structure to hold coordinates
typedef struct {
    double latitude;
    double longitude;
    double altitude; // in meters
} Coordinates;

// Structure to hold missile attributes
typedef struct {
    double weight;            // in kg
    double speed;             // in m/s
    double fuel;              // in kg
    double burnRate;          // in kg/s
    double thrust;            // in Newtons
    double maxAcceleration;   // in m/s²
    double maxDeceleration;   // in m/s²
    double maxTurnRate;       // in degrees/second
    double dragCoefficient;   // Dimensionless
    double fuelConsumptionNormal;  // in kg/s during normal flight
    double fuelConsumptionTurn;    // in kg/s during turns
} MissileAttributes;

// Structure to hold waypoint data
typedef struct {
    Coordinates position;           // Waypoint position
    double turnAngle;              // Angle to turn in degrees
    double approachSpeed;          // Speed when approaching this waypoint in m/s
    double departureSpeed;         // Speed after leaving this waypoint in m/s
    double timeToReach;            // Time to reach from previous point in seconds
    double distanceFromPrevious;   // Distance from previous point in km
    double bearingFromPrevious;    // Bearing from previous point in degrees
    double fuelConsumed;           // Fuel consumed to reach this waypoint in kg
    double gForce;                 // G-force experienced during turn
} Waypoint;

// Structure to hold trajectory data
typedef struct {
    Coordinates start;
    Coordinates end;
    MissileAttributes missile;
    double totalDistance;    // Total distance in km
    double totalTravelTime;  // Total travel time in seconds
    double initialBearing;   // Initial bearing in degrees
    int waypointCount;       // Number of waypoints
    Waypoint waypoints[MAX_WAYPOINTS]; // Array of waypoints
    double currentSpeed;     // Current speed in m/s
    double remainingFuel;    // Remaining fuel in kg
} TrajectoryData;

// Function declarations
double calculateDistance(Coordinates start, Coordinates end);
double calculateBearing(Coordinates start, Coordinates end);
double calculateTravelTime(double distance, double speed);
Coordinates calculateIntermediatePoint(Coordinates start, Coordinates end, double fraction);
TrajectoryData calculateTrajectory(Coordinates start, Coordinates end, MissileAttributes missile);

// New function declarations for waypoints and physics
void addWaypoint(TrajectoryData* trajectory, Coordinates position, double turnAngle);
void calculateWaypointEffects(TrajectoryData* trajectory, int waypointIndex);
void calculateFullTrajectory(TrajectoryData* trajectory);
double calculateTurnEffect(double speed, double turnAngle, double dragCoefficient);
double calculateGForce(double speed, double turnRadius);
Coordinates* generatePathPoints(TrajectoryData* trajectory, int* pointCount);


#endif /* TRAJECTORY_H */
