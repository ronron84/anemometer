# anemometer
anémometer and wind vane
// Dynamic gust detection logic
if (filteredSpeed - avg1Min > dynamicThreshold * 10) {
  gustDuration++;
  if (gustDuration >= 2) { // Valid gust if >4s (2 measurements)
    gustCount++;
    if (speedKmh > maxGustSpeed) maxGustSpeed = speedKmh;
  }
}
