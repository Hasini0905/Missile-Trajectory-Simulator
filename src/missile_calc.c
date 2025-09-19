#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "trajectory.h"

// Function to parse waypoints from a string
int parseWaypoints(const char* waypointStr, Coordinates waypoints[], double angles[], int maxWaypoints) {
    if (!waypointStr || strlen(waypointStr) == 0) {
        return 0; // No waypoints
    }
    
    int count = 0;
    const char* ptr = waypointStr;
    
    while (*ptr && count < maxWaypoints) {
        // Format: lat,lon,alt,angle|lat,lon,alt,angle|...
        double lat, lon, alt, angle;
        int parsed = sscanf(ptr, "%lf,%lf,%lf,%lf", &lat, &lon, &alt, &angle);
        
        if (parsed != 4) {
            break; // Invalid format
        }
        
        waypoints[count].latitude = lat;
        waypoints[count].longitude = lon;
        waypoints[count].altitude = alt;
        angles[count] = angle;
        count++;
        
        // Find the next waypoint separator
        ptr = strchr(ptr, '|');
        if (!ptr) break;
        ptr++; // Skip the separator
    }
    
    return count;
}

// Function to output trajectory data to a JSON file for the web frontend
void outputTrajectoryJSON(TrajectoryData trajectory, const char* outputFile) {
    FILE* file = fopen(outputFile, "w");
    if (!file) {
        fprintf(stderr, "Error opening output file\n");
        return;
    }
    
    // Write trajectory data as JSON
    fprintf(file, "{\n");
    fprintf(file, "  \"totalDistance\": %.6f,\n", trajectory.totalDistance);
    fprintf(file, "  \"totalTravelTime\": %.6f,\n", trajectory.totalTravelTime);
    fprintf(file, "  \"initialBearing\": %.6f,\n", trajectory.initialBearing);
    fprintf(file, "  \"currentSpeed\": %.6f,\n", trajectory.currentSpeed);
    fprintf(file, "  \"remainingFuel\": %.6f,\n", trajectory.remainingFuel);
    fprintf(file, "  \"start\": {\n");
    fprintf(file, "    \"latitude\": %.6f,\n", trajectory.start.latitude);
    fprintf(file, "    \"longitude\": %.6f,\n", trajectory.start.longitude);
    fprintf(file, "    \"altitude\": %.6f\n", trajectory.start.altitude);
    fprintf(file, "  },\n");
    fprintf(file, "  \"end\": {\n");
    fprintf(file, "    \"latitude\": %.6f,\n", trajectory.end.latitude);
    fprintf(file, "    \"longitude\": %.6f,\n", trajectory.end.longitude);
    fprintf(file, "    \"altitude\": %.6f\n", trajectory.end.altitude);
    fprintf(file, "  },\n");
    fprintf(file, "  \"missile\": {\n");
    fprintf(file, "    \"weight\": %.6f,\n", trajectory.missile.weight);
    fprintf(file, "    \"speed\": %.6f,\n", trajectory.missile.speed);
    fprintf(file, "    \"fuel\": %.6f,\n", trajectory.missile.fuel);
    fprintf(file, "    \"burnRate\": %.6f,\n", trajectory.missile.burnRate);
    fprintf(file, "    \"thrust\": %.6f,\n", trajectory.missile.thrust);
    fprintf(file, "    \"maxAcceleration\": %.6f,\n", trajectory.missile.maxAcceleration);
    fprintf(file, "    \"maxDeceleration\": %.6f,\n", trajectory.missile.maxDeceleration);
    fprintf(file, "    \"maxTurnRate\": %.6f,\n", trajectory.missile.maxTurnRate);
    fprintf(file, "    \"dragCoefficient\": %.6f\n", trajectory.missile.dragCoefficient);
    fprintf(file, "  },\n");
    
    // Write waypoints
    fprintf(file, "  \"waypoints\": [\n");
    for (int i = 0; i < trajectory.waypointCount; i++) {
        Waypoint waypoint = trajectory.waypoints[i];
        fprintf(file, "    {\n");
        fprintf(file, "      \"position\": {\n");
        fprintf(file, "        \"latitude\": %.6f,\n", waypoint.position.latitude);
        fprintf(file, "        \"longitude\": %.6f,\n", waypoint.position.longitude);
        fprintf(file, "        \"altitude\": %.6f\n", waypoint.position.altitude);
        fprintf(file, "      },\n");
        fprintf(file, "      \"turnAngle\": %.6f,\n", waypoint.turnAngle);
        fprintf(file, "      \"approachSpeed\": %.6f,\n", waypoint.approachSpeed);
        fprintf(file, "      \"departureSpeed\": %.6f,\n", waypoint.departureSpeed);
        fprintf(file, "      \"timeToReach\": %.6f,\n", waypoint.timeToReach);
        fprintf(file, "      \"distanceFromPrevious\": %.6f,\n", waypoint.distanceFromPrevious);
        fprintf(file, "      \"bearingFromPrevious\": %.6f,\n", waypoint.bearingFromPrevious);
        fprintf(file, "      \"fuelConsumed\": %.6f,\n", waypoint.fuelConsumed);
        fprintf(file, "      \"gForce\": %.6f\n", waypoint.gForce);
        fprintf(file, "    }%s\n", (i < trajectory.waypointCount - 1) ? "," : "");
    }
    fprintf(file, "  ],\n");
    
    // Calculate and write path points
    fprintf(file, "  \"path\": [\n");
    
    int pathPointCount = 0;
    Coordinates* pathPoints = generatePathPoints(&trajectory, &pathPointCount);
    
    if (pathPoints) {
        for (int i = 0; i < pathPointCount; i++) {
            fprintf(file, "    {\n");
            fprintf(file, "      \"latitude\": %.6f,\n", pathPoints[i].latitude);
            fprintf(file, "      \"longitude\": %.6f,\n", pathPoints[i].longitude);
            fprintf(file, "      \"altitude\": %.6f\n", pathPoints[i].altitude);
            fprintf(file, "    }%s\n", (i < pathPointCount - 1) ? "," : "");
        }
        
        free(pathPoints);
    }
    
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");
    
    fclose(file);
}

// Main function
int main(int argc, char* argv[]) {
    if (argc < 10) {
        printf("Usage: %s <start_lat> <start_lon> <start_alt> <end_lat> <end_lon> <end_alt> <weight> <speed> <output_file> [waypoints]\n", argv[0]);
        printf("Waypoints format: lat,lon,alt,angle|lat,lon,alt,angle|...\n");
        return 1;
    }
    
    // Parse command line arguments
    Coordinates start, end;
    MissileAttributes missile;
    
    start.latitude = atof(argv[1]);
    start.longitude = atof(argv[2]);
    start.altitude = atof(argv[3]);
    
    end.latitude = atof(argv[4]);
    end.longitude = atof(argv[5]);
    end.altitude = atof(argv[6]);
    
    missile.weight = atof(argv[7]);
    missile.speed = atof(argv[8]);
    missile.fuel = missile.weight * 0.7; // Assume 70% of weight is fuel
    missile.burnRate = missile.fuel / 60.0; // Burn all fuel in 60 seconds
    missile.thrust = missile.weight * 30.0; // Simple thrust calculation
    
    // Set advanced physics parameters
    missile.maxAcceleration = 30.0; // m/s²
    missile.maxDeceleration = 50.0; // m/s²
    missile.maxTurnRate = 20.0;     // degrees/second
    missile.dragCoefficient = 0.1;  // dimensionless
    missile.fuelConsumptionNormal = missile.burnRate;
    missile.fuelConsumptionTurn = missile.burnRate * 2.0;
    
    const char* outputFile = argv[9];
    
    // Calculate initial trajectory
    TrajectoryData trajectory = calculateTrajectory(start, end, missile);
    
    // Parse and add waypoints if provided
    if (argc > 10) {
        Coordinates waypointCoords[MAX_WAYPOINTS];
        double turnAngles[MAX_WAYPOINTS];
        
        int waypointCount = parseWaypoints(argv[10], waypointCoords, turnAngles, MAX_WAYPOINTS);
        
        for (int i = 0; i < waypointCount; i++) {
            addWaypoint(&trajectory, waypointCoords[i], turnAngles[i]);
        }
    }
    
    // Print trajectory information
    printf("Total distance: %.2f km\n", trajectory.totalDistance);
    printf("Total travel time: %.2f seconds\n", trajectory.totalTravelTime);
    printf("Initial bearing: %.2f degrees\n", trajectory.initialBearing);
    printf("Remaining fuel: %.2f kg\n", trajectory.remainingFuel);
    printf("Final speed: %.2f m/s\n", trajectory.currentSpeed);
    
    // Print waypoint information
    printf("\nWaypoints: %d\n", trajectory.waypointCount);
    for (int i = 0; i < trajectory.waypointCount; i++) {
        Waypoint wp = trajectory.waypoints[i];
        printf("Waypoint %d:\n", i + 1);
        printf("  Position: %.6f, %.6f, %.6f\n", wp.position.latitude, wp.position.longitude, wp.position.altitude);
        printf("  Turn angle: %.2f degrees\n", wp.turnAngle);
        printf("  Approach speed: %.2f m/s\n", wp.approachSpeed);
        printf("  Departure speed: %.2f m/s\n", wp.departureSpeed);
        printf("  G-force: %.2f g\n", wp.gForce);
        printf("  Distance from previous: %.2f km\n", wp.distanceFromPrevious);
        printf("  Time to reach: %.2f seconds\n", wp.timeToReach);
        printf("  Fuel consumed: %.2f kg\n", wp.fuelConsumed);
    }
    
    // Output JSON for web frontend
    outputTrajectoryJSON(trajectory, outputFile);
    
    return 0;
}
