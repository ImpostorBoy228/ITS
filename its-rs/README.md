```rust
let finals = std::fs::read("finals.all")?;

// finals.all -> (mjd, dut1) & spline
let (mjd, dut1) = its_rs::load_finals(&finals)?;
let spline = its_rs::build_spline(&mjd, &dut1);

// cumputing offset
let offset = its_rs::cumpute_offset(&mjd, &dut1, spline.as_deref());

// nsecs scince ITS EPOCH
let now_ns = std::time::SystemTime::now()
    .duration_since(std::time::UNIX_EPOCH)?
    .as_nanos() as u64;
let its_ns = its_rs::its_elapsed_ns(
    now_ns,
    offset,
    &mjd,
    &dut1,
    spline.as_deref(),
);

// breakdown its_ns 
let days = (its_ns / 1_000_000_000) / 86400;
let (y, m, d) = its_rs::format_its_ymd(days);
let sec_of_day = (its_ns / 1_000_000_000) % 86400;
println!("{y}y {m}m {d}d {}", its_rs::format_its_hms(sec_of_day));
```

### DUT1 and spline

- [`load_finals(&[u8]) -> Result<(Vec<f64>, Vec<f64>), String>`](src/lib.rs) — parses a `finals.all` buffer (MJD and DUT1 columns); lines with a blank DUT1 field are skipped.
- [`build_spline(&[f64], &[f64]) -> Option<Vec<f64>>`](src/lib.rs) — constructs a natural cubic spline (second derivatives); returns `None` when fewer than two points are available.
- [`interpol(mjd: f64, &mjd, &dut1, Option<&[f64]>) -> f64`](src/lib.rs) — DUT1 at an arbitrary MJD; linear interpolation when no spline is supplied, clamped to endpoints outside the data range.

### Astronomy

- [`sun_position(jd: f64) -> (f64, f64)`](src/lib.rs) — solar declination (deg) and equation of time (sec) for a Julian date.
- [`hour_angle(lat: f64, decl: f64, zenith: f64, sign: i32) -> f64`](src/lib.rs) — hour angle in hours; `-1.0` when the altitude condition cannot be met.
- [`compute_times(y: i32, m: i32, d: i32) -> (f64, f64, f64, bool)`](src/lib.rs) — sunset (sec of day), end of astronomical twilight (−108°), day length, and a `has_night` flag; all time fields are `-1.0` when there is no night.
- [`cumpute_offset(&mjd, &dut1, Option<&[f64]>) -> f64`](src/lib.rs) — epoch offset in UT1 seconds of day of the earliest nightfall over 1976–2026 (cached via `OnceLock`); `-1.0` when no night was found.

### Conversion and formatting

- [`its_elapsed_ns(timestamp_ns: u64, offset: f64, &mjd, &dut1, Option<&[f64]>) -> u64`](src/lib.rs) — UNIX nanoseconds → nanoseconds since the ITS epoch (u64 is valid until ~2554).
- [`format_its_ymd(day: i64) -> (i64, i64, i64)`](src/lib.rs) — ITS day number → (year, month 0–6, day of month 0–20); valid for negative days via floor division.
- [`format_its_hms(sec: u64) -> String`](src/lib.rs) — seconds of day → `"H:MM:SS"`.

[`finals.all`](https://datacenter.iers.org/data/latest/finals.all) is required

License: MIT
