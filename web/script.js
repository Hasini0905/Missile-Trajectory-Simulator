document.addEventListener('DOMContentLoaded', function() {
    // Minimal working map
    const map = L.map('map').setView([20, 78], 5);
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        attribution: '&copy; OpenStreetMap contributors'
    }).addTo(map);

    // State
    let points = [];
    let markers = [];
    let trajectoryLine = null;
    let missileMarker = null;
    let missileAnimation = null;
    let logData = [];

    // Sidebar elements
    const manualLatInput = document.getElementById('manual-lat');
    const manualLngInput = document.getElementById('manual-lng');
    const manualAltInput = document.getElementById('manual-alt');
    const mapAltInput = document.getElementById('map-alt');
    const speedInput = document.getElementById('speed-input');
    const addPointManualBtn = document.getElementById('add-point-manual');
    const addPointMapBtn = document.getElementById('add-point-map');
    const clearPointsBtn = document.getElementById('clear-points');
    const pointsListDiv = document.getElementById('points-list');
    const calcTrajectoryBtn = document.getElementById('calculate-trajectory');
    const launchMissileBtn = document.getElementById('launch-missile');
    const exportLogBtn = document.getElementById('export-log');
    const exportIntervalInput = document.getElementById('export-interval');
    const exportIntervalType = document.getElementById('export-interval-type');
    const distanceOutput = document.getElementById('distance');
    const travelTimeOutput = document.getElementById('travel-time');
    const currentPositionOutput = document.getElementById('current-position');
    const currentSpeedOutput = document.getElementById('current-speed');
    const etaOutput = document.getElementById('eta');

    // Helper: calculate turning angle at a point (in degrees)
    function calculateTurnAngle(idx) {
        if (idx < 2 || idx >= points.length) return null;
        const p0 = points[idx - 2];
        const p1 = points[idx - 1];
        const p2 = points[idx];
        // Convert to radians
        const toRad = x => x * Math.PI / 180;
        const toDeg = x => x * 180 / Math.PI;
        // Vector from p0 to p1
        const v1 = [p1.lat - p0.lat, p1.lng - p0.lng];
        // Vector from p1 to p2
        const v2 = [p2.lat - p1.lat, p2.lng - p1.lng];
        // Calculate angle between v1 and v2
        const dot = v1[0]*v2[0] + v1[1]*v2[1];
        const mag1 = Math.sqrt(v1[0]*v1[0] + v1[1]*v1[1]);
        const mag2 = Math.sqrt(v2[0]*v2[0] + v2[1]*v2[1]);
        if (mag1 === 0 || mag2 === 0) return null;
        let angle = Math.acos(Math.max(-1, Math.min(1, dot / (mag1 * mag2))));
        return toDeg(angle);
    }

    // Helper: update sidebar list with delete buttons and turning angles
    function updateSidebarList() {
        let html = '';
        points.forEach((pt, idx) => {
            let angleStr = '';
            if (idx >= 2) {
                const angle = calculateTurnAngle(idx);
                if (angle !== null) {
                    angleStr = ` | Turn: ${angle.toFixed(1)}°`;
                }
            }
            html += `<div>Point ${idx+1}: Lat ${pt.lat.toFixed(4)}, Lng ${pt.lng.toFixed(4)}, Alt ${pt.alt}${angleStr} <button class='delete-point' data-idx='${idx}'>Delete</button></div>`;
        });
        pointsListDiv.innerHTML = html;
        // Add delete event listeners
        Array.from(pointsListDiv.querySelectorAll('.delete-point')).forEach(btn => {
            btn.onclick = function() {
                const idx = parseInt(this.getAttribute('data-idx'));
                points.splice(idx, 1);
                map.removeLayer(markers[idx]);
                markers.splice(idx, 1);
                if (trajectoryLine) { map.removeLayer(trajectoryLine); trajectoryLine = null; }
                updateSidebarList();
            };
        });
    }

    // Helper: clear all markers and lines
    function clearAllMarkersAndLines() {
        markers.forEach(m => map.removeLayer(m));
        markers = [];
        if (trajectoryLine) {
            map.removeLayer(trajectoryLine);
            trajectoryLine = null;
        }
        if (missileMarker) {
            map.removeLayer(missileMarker);
            missileMarker = null;
        }
        if (missileAnimation) {
            cancelAnimationFrame(missileAnimation);
            missileAnimation = null;
        }
        logData = [];
        currentPositionOutput.textContent = '-';
        currentSpeedOutput.textContent = '-';
        etaOutput.textContent = '-';
        travelTimeOutput.textContent = '-';
    }

    // Add point by map click
    let mapClickMode = false;
    addPointMapBtn.addEventListener('click', function() {
        mapClickMode = !mapClickMode;
        addPointMapBtn.textContent = mapClickMode ? 'Stop Adding Points on Map' : 'Add Point (Map)';
    });
    map.on('click', function(e) {
        if (!mapClickMode) return;
        const alt = parseFloat(mapAltInput.value) || 0;
        const pt = { lat: e.latlng.lat, lng: e.latlng.lng, alt };
        points.push(pt);
        const marker = L.marker([pt.lat, pt.lng]).addTo(map).bindPopup(`Point ${points.length}`);
        markers.push(marker);
        updateSidebarList();
    });

    // Add point by manual entry (fix: always add to map and list)
    addPointManualBtn.addEventListener('click', function() {
        const lat = parseFloat(manualLatInput.value);
        const lng = parseFloat(manualLngInput.value);
        const alt = parseFloat(manualAltInput.value) || 0;
        if (isNaN(lat) || isNaN(lng)) {
            alert('Please enter valid latitude and longitude.');
            return;
        }
        const pt = { lat, lng, alt };
        points.push(pt);
        const marker = L.marker([pt.lat, pt.lng]).addTo(map).bindPopup(`Point ${points.length}`);
        markers.push(marker);
        updateSidebarList();
        manualLatInput.value = '';
        manualLngInput.value = '';
        manualAltInput.value = '';
    });

    // Clear all points
    clearPointsBtn.addEventListener('click', function() {
        points = [];
        clearAllMarkersAndLines();
        updateSidebarList();
        distanceOutput.textContent = '-';
    });

    // Calculate and draw trajectory: first segment straight, rest are Catmull-Rom spline
    function calculateDistance(lat1, lng1, lat2, lng2) {
        const R = 6371; // km
        const dLat = (lat2 - lat1) * Math.PI / 180;
        const dLng = (lng2 - lng1) * Math.PI / 180;
        const a = Math.sin(dLat/2) * Math.sin(dLat/2) +
                  Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
                  Math.sin(dLng/2) * Math.sin(dLng/2);
        const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a));
        return R * c;
    }

    // Catmull-Rom Spline interpolation
    function catmullRomSpline(pointsArr, numSegments = 20) {
        if (pointsArr.length < 3) return pointsArr.map(p => [p.lat, p.lng, p.alt]);
        let splinePoints = [];
        // First segment: straight
        splinePoints.push([pointsArr[0].lat, pointsArr[0].lng, pointsArr[0].alt]);
        splinePoints.push([pointsArr[1].lat, pointsArr[1].lng, pointsArr[1].alt]);
        // For each next segment, use Catmull-Rom
        for (let i = 1; i < pointsArr.length - 2; i++) {
            const p0 = pointsArr[i - 1];
            const p1 = pointsArr[i];
            const p2 = pointsArr[i + 1];
            const p3 = pointsArr[i + 2];
            for (let t = 0; t <= 1; t += 1 / numSegments) {
                const t2 = t * t;
                const t3 = t2 * t;
                const lat = 0.5 * ((2 * p1.lat) +
                    (-p0.lat + p2.lat) * t +
                    (2*p0.lat - 5*p1.lat + 4*p2.lat - p3.lat) * t2 +
                    (-p0.lat + 3*p1.lat - 3*p2.lat + p3.lat) * t3);
                const lng = 0.5 * ((2 * p1.lng) +
                    (-p0.lng + p2.lng) * t +
                    (2*p0.lng - 5*p1.lng + 4*p2.lng - p3.lng) * t2 +
                    (-p0.lng + 3*p1.lng - 3*p2.lng + p3.lng) * t3);
                const alt = 0.5 * ((2 * p1.alt) +
                    (-p0.alt + p2.alt) * t +
                    (2*p0.alt - 5*p1.alt + 4*p2.alt - p3.alt) * t2 +
                    (-p0.alt + 3*p1.alt - 3*p2.alt + p3.alt) * t3);
                splinePoints.push([lat, lng, alt]);
            }
        }
        // Last segment: straight
        splinePoints.push([pointsArr[pointsArr.length-2].lat, pointsArr[pointsArr.length-2].lng, pointsArr[pointsArr.length-2].alt]);
        splinePoints.push([pointsArr[pointsArr.length-1].lat, pointsArr[pointsArr.length-1].lng, pointsArr[pointsArr.length-1].alt]);
        return splinePoints;
    }

    let trajectoryPath = [];
    let totalTravelTime = 0;

    calcTrajectoryBtn.addEventListener('click', function() {
        if (points.length < 2) {
            alert('Add at least two points to calculate trajectory.');
            return;
        }
        // Remove old line
        if (trajectoryLine) {
            map.removeLayer(trajectoryLine);
        }
        let pathLatLngs = [];
        if (points.length === 2) {
            // Only straight line
            pathLatLngs = [[points[0].lat, points[0].lng, points[0].alt], [points[1].lat, points[1].lng, points[1].alt]];
        } else {
            // First segment straight, rest Catmull-Rom
            pathLatLngs = catmullRomSpline(points);
        }
        trajectoryPath = pathLatLngs;
        trajectoryLine = L.polyline(pathLatLngs.map(p=>[p[0],p[1]]), {color: 'red', weight: 3}).addTo(map);
        // Calculate total distance
        let totalDist = 0;
        for (let i = 1; i < pathLatLngs.length; i++) {
            totalDist += calculateDistance(pathLatLngs[i-1][0], pathLatLngs[i-1][1], pathLatLngs[i][0], pathLatLngs[i][1]);
        }
        distanceOutput.textContent = (totalDist * 1000).toFixed(0) + ' m';

        // Calculate travel time
        const speed = parseFloat(speedInput.value) || 1;
        totalTravelTime = totalDist * 1000 / speed; // seconds
        travelTimeOutput.textContent = totalTravelTime.toFixed(1) + ' s';
    });

    // Missile simulation
    launchMissileBtn.addEventListener('click', function() {
        if (!trajectoryPath || trajectoryPath.length < 2) {
            alert('Calculate trajectory first.');
            return;
        }
        if (missileMarker) {
            map.removeLayer(missileMarker);
            missileMarker = null;
        }
        if (missileAnimation) {
            cancelAnimationFrame(missileAnimation);
            missileAnimation = null;
        }
        const speed = parseFloat(speedInput.value) || 1;
        let t = 0;
        let totalDist = 0;
        let distArr = [0];
        for (let i = 1; i < trajectoryPath.length; i++) {
            totalDist += calculateDistance(trajectoryPath[i-1][0], trajectoryPath[i-1][1], trajectoryPath[i][0], trajectoryPath[i][1]);
            distArr.push(totalDist);
        }
        const totalDistMeters = totalDist * 1000;
        let startTime = null;
        logData = [];
        function animate(ts) {
            if (!startTime) startTime = ts;
            const elapsed = (ts - startTime) / 1000; // seconds
            const progress = Math.min(elapsed * speed, totalDistMeters);
            // Find segment
            let segIdx = 1;
            while (segIdx < distArr.length && distArr[segIdx]*1000 < progress) segIdx++;
            segIdx = Math.min(segIdx, trajectoryPath.length-1);
            const prev = trajectoryPath[segIdx-1];
            const next = trajectoryPath[segIdx];
            const segDist = calculateDistance(prev[0], prev[1], next[0], next[1]) * 1000;
            const segProgress = segDist === 0 ? 0 : (progress - distArr[segIdx-1]*1000) / segDist;
            const lat = prev[0] + (next[0] - prev[0]) * segProgress;
            const lng = prev[1] + (next[1] - prev[1]) * segProgress;
            const alt = prev[2] + (next[2] - prev[2]) * segProgress;
            if (!missileMarker) {
                missileMarker = L.marker([lat, lng], {icon: L.divIcon({className:'missile-marker',html:'🚀',iconSize:[24,24]})}).addTo(map);
            } else {
                missileMarker.setLatLng([lat, lng]);
            }
            // Live info
            currentPositionOutput.textContent = `Lat: ${lat.toFixed(4)}, Lng: ${lng.toFixed(4)}, Alt: ${alt.toFixed(0)}m`;
            currentSpeedOutput.textContent = speed + ' m/s';
            const eta = Math.max(0, totalTravelTime - elapsed);
            etaOutput.textContent = eta.toFixed(1) + ' s';
            // Log
            logData.push({time: elapsed, lat, lng, alt, speed, eta, dist: progress});
            if (progress < totalDistMeters) {
                missileAnimation = requestAnimationFrame(animate);
            } else {
                missileAnimation = null;
            }
        }
        missileAnimation = requestAnimationFrame(animate);
    });

    // Export logbook as CSV with user-selected interval
    exportLogBtn.addEventListener('click', function() {
        if (!logData.length) {
            alert('No log data to export. Launch the missile first.');
            return;
        }
        const interval = parseFloat(exportIntervalInput.value) || 1;
        const intervalType = exportIntervalType.value;
        let csv = 'time,lat,lng,alt,speed,eta,dist\n';
        let lastValue = 0;
        for (let i = 0; i < logData.length; i++) {
            const d = logData[i];
            let value = intervalType === 'seconds' ? d.time : d.dist;
            if (value - lastValue >= interval || i === 0) {
                csv += `${d.time.toFixed(2)},${d.lat.toFixed(6)},${d.lng.toFixed(6)},${d.alt.toFixed(1)},${d.speed},${d.eta.toFixed(1)},${d.dist.toFixed(1)}\n`;
                lastValue = value;
            }
        }
        // Always add last point
        const last = logData[logData.length-1];
        csv += `${last.time.toFixed(2)},${last.lat.toFixed(6)},${last.lng.toFixed(6)},${last.alt.toFixed(1)},${last.speed},${last.eta.toFixed(1)},${last.dist.toFixed(1)}\n`;
        const blob = new Blob([csv], {type: 'text/csv'});
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'missile_log.csv';
        a.click();
        URL.revokeObjectURL(url);
    });

    // Initial sidebar update
    updateSidebarList();
});



