use std::io::{BufRead, BufReader};
use std::time::{SystemTime, UNIX_EPOCH};
// use num_bigint::BigUint;
// use num_traits::{One, Zero}; 
// blazing fast until year 2554 

const SECSPERDAY: u64 = 24*60*60;
const ITS_YEAR_DAYS: i64 = 147;
const ITS_MONTH_DAYS: i64 = 21;
const LON: f64 = 82.93;
const LAT: f64 = 55.03;
const ZENITH: f64 = 108.0;
const PI: f64 = std::f64::consts::PI;
const EPOCH_UNIX: f64 = 1782086400.0;

fn dut1_field_blank(line: &str) -> bool { 
    // this checks DUT1 field is blank
    if line.len() < 69 { return true; }
    line[58..69].chars().all(|c| c.is_whitespace())
}

pub fn load_finals(finals: &[u8]) -> Result<(Vec<f64>, Vec<f64>), String> {
    // this returns mjd and dut1 from finals.all
    let veclen = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_secs() / SECSPERDAY - 1096;

    let reader = BufReader::new(finals);
    let mut mjd_vec = Vec::with_capacity(veclen.try_into().unwrap());
    let mut dut1_vec = Vec::with_capacity(veclen.try_into().unwrap());

    for line in reader.lines() {
        let line = line.map_err(|e| e.to_string())?;
        if line.len() < 69 { continue; }
        if line.starts_with("MJD") { continue; }
        if dut1_field_blank(&line) { continue; }

        let mjd = line[7..15].trim().parse::<f64>().unwrap_or(0.0);
        let dut1 = line[58..69].trim().parse::<f64>().unwrap_or(0.0);
        if mjd > 0.0 && dut1 > -10.0 && dut1 < 10.0 {
            mjd_vec.push(mjd);
            dut1_vec.push(dut1);
        }
    }

    Ok((mjd_vec, dut1_vec))    
}

pub fn build_spline(
    // this builds natural spline and returns it
    mjd_vec: Vec<f64>,
    dut1_vec: Vec<f64>) 
    -> Option<Vec<f64>> {
    let n = mjd_vec.len();
    if n < 2 || mjd_vec.len() != dut1_vec.len() {
        return None;
    }
    let mut h = Vec::with_capacity(n-1);
    let mut b = Vec::with_capacity(n-1);
    for i in 0..n - 1 {
        let hi = mjd_vec[i + 1] - mjd_vec[i];
        h.push(hi);
        b.push((dut1_vec[i + 1] - dut1_vec[i]) / hi);
    }

    // malloc(not quite)
    let mut d = vec![0.0; n];
    let mut l = vec![0.0; n];
    let mut mu = vec![0.0; n];
    let mut z = vec![0.0; n];

    // edging rules
    d[0] = 1.0;
    l[0] = 0.0;
    mu[0] = 0.0;
    z[0] = 0.0;
    for i in 1..n - 1 {
        let h_im1 = h[i - 1];
        let h_i = h[i];
        l[i] = h_im1 / (h_im1 + h_i);
        mu[i] = h_i / (h_im1 + h_i);
        d[i] = 2.0;
        z[i] = 6.0 * ((b[i] - b[i - 1]) / (h_im1 + h_i));
    }

    let last = n - 1;
    d[last] = 1.0;
    l[last] = 0.0;
    mu[last] = 0.0;
    z[last] = 0.0;
    
    // cursed technique
    for i in 1..n {
        let factor = l[i] / d[i - 1];
        d[i] -= factor * mu[i - 1];
        z[i] -= factor * z[i - 1];
    }

    // reverse cursed technique
    let mut second_deriv = vec![0.0; n];
    second_deriv[last] = z[last] / d[last];
    for i in (0..=n - 2).rev() {
        second_deriv[i] = (z[i] - mu[i] * second_deriv[i + 1]) / d[i];
    }

    Some(second_deriv)
}

pub fn interpol(
    mjd: f64,
    mjd_vec: Vec<f64>,
    dut1_vec: Vec<f64>,
    second_deriv: Option<Vec<f64>>) 
    -> f64 {
    let n = mjd_vec.len();
    if n==0 {return 0.0}
    if mjd <= mjd_vec[0] {return dut1_vec[0]}
    if mjd >= mjd_vec[n-1] {return dut1_vec[n-1]}
    let mut i = 0;
    while i < n - 1 && mjd_vec[i + 1] < mjd {
        i += 1;
    }
    let h = mjd_vec[i + 1] - mjd_vec[i];
    let t = (mjd - mjd_vec[i]) / h;
    let y0 = dut1_vec[i];
    let y1 = dut1_vec[i + 1];

    let linear = (1.0 - t) * y0 + t * y1;

    let second_deriv = match second_deriv {
        Some(s) => s,
        None => return linear,
    };

    let s0 = second_deriv[i];
    let s1 = second_deriv[i + 1];
    let one_minus_t = 1.0 - t;
    let t_cube = t * t * t;

    let correction = ((one_minus_t * one_minus_t * one_minus_t - one_minus_t) * s0
        + (t_cube - t) * s1)
        * h * h
        / 6.0;

    linear + correction
}

fn jdn(y: i32, m: i32, d: i32) -> f64 {
    let (mut y, mut m) = (y, m);
    if m <= 2 { y -= 1; m += 12; }
    let a = y / 100;
    let b = 2 - a + a / 4;
    (365.25 * (y + 4716) as f64).floor()
        + (30.6001 * (m + 1) as f64).floor()
        + d as f64 + b as f64 - 1524.5
}

pub fn sun_position(jd: f64) -> (f64, f64) {
    let t = (jd - 2451545.0) / 36525.0;
    let l0 = (280.46646 + 36000.76983 * t + 0.0003032 * t * t) % 360.0;
    let l0 = if l0 < 0.0 { l0 + 360.0 } else { l0 };
    let m = (357.52911 + 35999.05029 * t - 0.0001537 * t * t) % 360.0;
    let m = if m < 0.0 { m + 360.0 } else { m };
    let c = (1.914602 - 0.004817 * t - 0.000014 * t * t) * (m * PI / 180.0).sin()
          + (0.019993 - 0.000101 * t) * (2.0 * m * PI / 180.0).sin()
          + 0.000289 * (3.0 * m * PI / 180.0).sin();
    let sun_lon = l0 + c;
    let obliq = 23.439291 - 0.0130042 * t;
    let alpha = (obliq * PI / 180.0).cos() * (sun_lon * PI / 180.0).sin();
    let alpha = alpha.atan2((sun_lon * PI / 180.0).cos()) * 180.0 / PI;
    let alpha = (alpha % 360.0 + 360.0) % 360.0;
    let delta = ((obliq * PI / 180.0).sin() * (sun_lon * PI / 180.0).sin()).asin() * 180.0 / PI;
    let mut e = l0 - alpha;
    if e < -180.0 { e += 360.0; }
    if e > 180.0 { e -= 360.0; }
    (delta, e * 4.0)
}

pub fn hour_angle(lat: f64, decl: f64, zenith: f64, sign: i32) -> f64 {
    let cos_ha = (zenith * PI / 180.0).cos() - (lat * PI / 180.0).sin() * (decl * PI / 180.0).sin();
    let cos_ha = cos_ha / ((lat * PI / 180.0).cos() * (decl * PI / 180.0).cos());
    if cos_ha < -1.0 || cos_ha > 1.0 {
        return -1.0;
    }
    let ha = cos_ha.acos() * 180.0 / PI / 15.0;
    (sign as f64) * ha
}

pub fn compute_times(y: i32, m: i32, d: i32) -> (f64, f64, f64, bool) {
    let jd = jdn(y, m, d) - 0.5;
    let (decl, eq_time) = sun_position(jd);
    let noon = 12.0 - LON / 15.0 - eq_time / 60.0;
    let ha_sunset = hour_angle(LAT, decl, 90.833, 1);
    let ha_twilight = hour_angle(LAT, decl, ZENITH, 1);
    let has_night = ha_twilight > 0.0;
    if !has_night {
        return (-1.0, -1.0, 0.0, false);
    }
    let mut sunset = noon + ha_sunset;
    let mut twilight = noon + ha_twilight;
    if sunset < 0.0 { sunset += 24.0; }
    if twilight < 0.0 { twilight += 24.0; }
    if sunset >= 24.0 { sunset -= 24.0; }
    if twilight >= 24.0 { twilight -= 24.0; }
    (sunset * 3600.0, twilight * 3600.0, 2.0 * ha_sunset, true)
}

pub fn compute_earliest_night() -> (f64, i32, i32, i32) {
    let mut min_twilight = 1e9;
    let mut best_y = 0;
    let mut best_m = 0;
    let mut best_d = 0;
    for y in 1976..=2026 {
        for m in 1..=12 {
            let dim = if m == 2 {
                if y % 4 == 0 && (y % 100 != 0 || y % 400 == 0) { 29 } else { 28 }
            } else if m == 4 || m == 6 || m == 9 || m == 11 {
                30
            } else {
                31
            };
            for d in 1..=dim {
                let (_, twilight, _, has_night) = compute_times(y, m, d);
                if !has_night { continue; }
                if twilight < min_twilight {
                    min_twilight = twilight;
                    best_y = y;
                    best_m = m;
                    best_d = d;
                }
            }
        }
    }
    if min_twilight < 1e9 {
        (min_twilight, best_y, best_m, best_d)
    } else {
        (-1.0, 0, 0, 0)
    }
}

pub fn cumpute_offset(
    mjd_vec: &[f64],
    dut1_vec: &[f64],
    second_deriv: Option<Vec<f64>>) 
    -> f64 {
    let (twi, y, m, d) = compute_earliest_night();
    if twi < 0.0 { return -1.0; }
    if mjd_vec.is_empty() { return twi; }
    let mjd = jdn(y, m, d) - 2400000.5;
    twi + interpol(mjd, mjd_vec.to_vec(), dut1_vec.to_vec(), second_deriv)
}

pub fn its_elapsed_ns( // update to bigint after 528 years
    timestamp_ns: u64,
    offset: f64,
    mjd_vec: Vec<f64>,
    dut1_vec: Vec<f64>,
    second_deriv: Option<Vec<f64>>) 
    ->  u64 {
    let epoch_ns = (EPOCH_UNIX as u64) * 1_000_000_000;
    let offset_ns = (offset * 1_000_000_000.0) as i64;
    let epoch_dut1_ns = (1_000_000_000.0 * interpol(EPOCH_UNIX / SECSPERDAY as f64 + 40587.0, mjd_vec, dut1_vec, second_deriv)) as i64;
    (timestamp_ns as i64 - epoch_ns as i64 - offset_ns + epoch_dut1_ns) as u64
}

pub fn format_its_hms(sec: u64) -> String {
    let h: u64 = sec/3600;
    let m: u64 = sec%3600 / 60;
    let s: u64 = sec%60;
    format!("{}:{}:{}", h, m, s)
}

pub fn format_its_ymd(day: i64) -> (i64, i64, i64) {
    let mut y = day / ITS_YEAR_DAYS;
    let mut rem = day % ITS_YEAR_DAYS;
    if day < 0 && rem != 0 {
        y -= 1;
        rem += ITS_YEAR_DAYS;
    }
    let m = rem / ITS_MONTH_DAYS;
    (y, m, rem % ITS_MONTH_DAYS)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn format_its_ymd_golden() {
        let cases: [(i64, i64, i64, i64); 13] = [
            (0, 0, 0, 0),
            (1, 0, 0, 1),
            (20, 0, 0, 20),
            (21, 0, 1, 0),
            (146, 0, 6, 20),
            (147, 1, 0, 0),
            (148, 1, 0, 1),
            (294, 2, 0, 0),
            (-1, -1, 6, 20),
            (-21, -1, 6, 0),
            (-22, -1, 5, 20),
            (-147, -1, 0, 0),
            (-148, -2, 6, 20),
        ];
        for &(day, ey, em, edr) in &cases {
            let (y, m, dr) = format_its_ymd(day);
            assert_eq!((y, m, dr), (ey, em, edr),
                "format_its_ymd({}) got ({},{},{}) want ({},{},{})", day, y, m, dr, ey, em, edr);
        }
    }

    #[test]
    fn format_its_ymd_roundtrip() {
        for day in -500..=500 {
            let (y, m, dr) = format_its_ymd(day);
            assert_eq!(day, y * ITS_YEAR_DAYS + m * ITS_MONTH_DAYS + dr,
                "reconstruct day={} y={} m={} dr={}", day, y, m, dr);
            assert!(m >= 0 && m <= 6, "months range day={} m={}", day, m);
            assert!(dr >= 0 && dr <= 20, "days_rem range day={} dr={}", day, dr);
        }
    }

    #[test]
    fn format_its_hms_golden() {
        assert_eq!(format_its_hms(44193), "12:16:33");
        assert_eq!(format_its_hms(0), "0:0:0");
        assert_eq!(format_its_hms(3661), "1:1:1");
        assert_eq!(format_its_hms(86399), "23:59:59");
        assert_eq!(format_its_hms(86400), "24:0:0");
    }

    #[test]
    fn format_its_hms_property() {
        for i in 0..100u64 {
            let sec = i * 1237;
            let s = format_its_hms(sec);
            let h = sec / 3600;
            let m = sec % 3600 / 60;
            let ss = sec % 60;
            assert_eq!(s, format!("{}:{}:{}", h, m, ss));
        }
    }

    #[test]
    fn jdn_anchors() {
        assert!((jdn(1970, 1, 1) - 2440587.5).abs() < 1e-9);
        assert!((jdn(2000, 1, 1) - 2451544.5).abs() < 1e-9);
        assert!((jdn(2026, 6, 22) - 2461213.5).abs() < 1e-9);
        assert!((jdn(1970, 1, 1) - 2400000.5 - 40587.0).abs() < 1e-9);
    }

    #[test]
    fn jdn_adjacency() {
        for doy in 1..365 {
            let (m1, d1) = doy_to_md(2026, doy);
            let (m2, d2) = doy_to_md(2026, doy + 1);
            assert!((jdn(2026, m2, d2) - jdn(2026, m1, d1) - 1.0).abs() < 1e-9,
                "doy {} -> {} gap != 1", doy, doy + 1);
        }
    }

    fn doy_to_md(year: i32, doy: i32) -> (i32, i32) {
        let mdays = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
        let leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        let mut rem = doy;
        for i in 0..12 {
            let dim = mdays[i] + if i == 1 && leap { 1 } else { 0 };
            if rem <= dim { return (i as i32 + 1, rem); }
            rem -= dim;
        }
        (12, 31)
    }

    #[test]
    fn jdn_leap_year() {
        assert!((jdn(2000, 3, 1) - jdn(2000, 2, 29) - 1.0).abs() < 1e-9);
        assert!((jdn(2000, 3, 1) - jdn(2000, 2, 28) - 2.0).abs() < 1e-9);
        assert!((jdn(1900, 3, 1) - jdn(1900, 2, 28) - 1.0).abs() < 1e-9);
        assert!((jdn(2024, 3, 1) - jdn(2024, 2, 29) - 1.0).abs() < 1e-9);
        assert!((jdn(2026, 3, 1) - jdn(2026, 2, 28) - 1.0).abs() < 1e-9);
    }

    #[test]
    fn sun_position_solstice() {
        let (decl, eq) = sun_position(jdn(2026, 6, 22));
        assert!(decl >= 23.0 && decl <= 24.0, "summer solstice decl={}", decl);
        assert!(eq.abs() < 1020.0, "summer eq_time={}", eq);

        let (decl, eq) = sun_position(jdn(2026, 12, 22));
        assert!(decl >= -24.0 && decl <= -23.0, "winter solstice decl={}", decl);
        assert!(eq.abs() < 1020.0, "winter eq_time={}", eq);

        let (decl, _) = sun_position(jdn(2000, 3, 20));
        assert!(decl >= -1.0 && decl <= 1.0, "vernal equinox decl={}", decl);
    }

    #[test]
    fn hour_angle_polar() {
        assert!((hour_angle(66.5, 23.44, 90.833, 1) - (-1.0)).abs() < 1e-12);
        assert!((hour_angle(90.0, 23.0, 90.833, 1) - (-1.0)).abs() < 1e-12);
        assert!(hour_angle(0.0, 0.0, 90.833, 1) > 0.0);
        assert!(hour_angle(0.0, 0.0, 90.833, -1) < 0.0);
        assert!((hour_angle(90.0, -23.0, 90.833, 1) - (-1.0)).abs() < 1e-12);
        assert!(hour_angle(0.0, 23.44, 90.833, 1) > 0.0);
    }

    #[test]
    fn compute_times_golden() {
        let (_, _, _, has_night) = compute_times(2026, 6, 22);
        assert!(!has_night, "summer solstice should have no night");

        let (sunset, twi, daylen, has_night) = compute_times(2026, 12, 12);
        assert!(has_night, "dec 12 should have night");
        assert!(sunset >= 0.0 && sunset < twi, "sunset={} twi={}", sunset, twi);
        assert!(twi <= 86400.0, "twilight={}", twi);
        assert!(daylen > 0.0 && daylen <= 86400.0, "daylen={}", daylen);
    }

    #[test]
    fn compute_times_sweep() {
        for doy in (1..=365).step_by(7) {
            let (m, d) = doy_to_md(2026, doy);
            let (s, t, dl, hn) = compute_times(2026, m, d);
            let (dc, _) = sun_position(jdn(2026, m, d));
            assert!(dc >= -23.5 && dc <= 23.5, "decl out of range doy={} decl={}", doy, dc);
            if hn {
                assert!(s >= 0.0 && s < t && t <= 86400.0,
                    "times out of order doy={} sunset={} twi={}", doy, s, t);
                assert!(dl > 0.0 && dl <= 86400.0, "daylen out of range doy={} dl={}", doy, dl);
            } else {
                assert_eq!(s, -1.0, "no-night sunset must be -1, doy={}", doy);
                assert_eq!(t, -1.0, "no-night twi must be -1, doy={}", doy);
                assert_eq!(dl, 0.0, "no-night daylen must be 0, doy={}", doy);
            }
        }
    }

    #[test]
    fn compute_earliest_night_golden() {
        let (_, y, m, d) = compute_earliest_night();
        assert_eq!((y, m, d), (2026, 12, 12));
    }

    #[test]
    fn dut1_field_blank_test() {
        assert!(super::dut1_field_blank("short"));
        assert!(!super::dut1_field_blank(&"x".repeat(69)));
        let mut line = " ".repeat(69);
        line.replace_range(58..69, "  0.1234567");
        assert!(!super::dut1_field_blank(&line));
        let all_space = " ".repeat(69);
        assert!(super::dut1_field_blank(&all_space));
    }

    #[test]
    fn load_finals_blank_field_skip() {
        let mut lines: Vec<String> = Vec::new();
        let mut line1 = " ".repeat(70);
        line1.replace_range(0..7, "73 1 1");
        line1.replace_range(7..15, &format!("{:8.2}", 60000.0));
        line1.replace_range(58..69, &format!("{:11.7}", 0.1));
        lines.push(line1);
        let mut line2 = " ".repeat(70);
        line2.replace_range(0..7, "73 1 1");
        line2.replace_range(7..15, &format!("{:8.2}", 60001.0));
        line2.replace_range(58..69, "           ");
        lines.push(line2);
        let mut line3 = " ".repeat(70);
        line3.replace_range(0..7, "73 1 1");
        line3.replace_range(7..15, &format!("{:8.2}", 60002.0));
        line3.replace_range(58..69, &format!("{:11.7}", 0.2));
        lines.push(line3);
        let data = lines.join("\n").into_bytes();
        let (mjds, duts) = load_finals(&data).unwrap();
        assert_eq!(mjds.len(), 2);
        assert!((mjds[0] - 60000.0).abs() < 1e-6);
        assert!((mjds[1] - 60002.0).abs() < 1e-6);
        assert!((duts[0] - 0.1).abs() < 1e-6);
        assert!((duts[1] - 0.2).abs() < 1e-6);
    }

    #[test]
    fn build_spline_insufficient_knots() {
        assert!(build_spline(vec![], vec![]).is_none());
        assert!(build_spline(vec![1.0], vec![1.0]).is_none());
    }

    #[test]
    fn build_spline_and_interpol() {
        let mjds = vec![60000.0, 60001.0, 60002.0, 60003.0];
        let duts = vec![0.10, 0.20, 0.10, 0.00];
        let sd = build_spline(mjds.clone(), duts.clone()).unwrap();

        let eps = 1e-6;
        let v = interpol(60000.0, mjds.clone(), duts.clone(), Some(sd.clone()));
        assert!((v - 0.10).abs() < eps, "interp(60000)={}", v);
        let v = interpol(60001.0, mjds.clone(), duts.clone(), Some(sd.clone()));
        assert!((v - 0.20).abs() < eps, "interp(60001)={}", v);
        let v = interpol(60002.0, mjds.clone(), duts.clone(), Some(sd.clone()));
        assert!((v - 0.10).abs() < eps, "interp(60002)={}", v);
        let v = interpol(60003.0, mjds.clone(), duts.clone(), Some(sd.clone()));
        assert!((v - 0.00).abs() < eps, "interp(60003)={}", v);

        let v = interpol(60000.5, mjds.clone(), duts.clone(), Some(sd.clone()));
        assert!(v > 0.10 && v < 0.20, "midpoint 60000.5={}", v);

        let v = interpol(59999.0, mjds.clone(), duts.clone(), Some(sd.clone()));
        assert!((v - 0.10).abs() < eps, "below first={}", v);
        let v = interpol(60004.0, mjds.clone(), duts.clone(), Some(sd.clone()));
        assert!((v - 0.00).abs() < eps, "above last={}", v);
    }

    #[test]
    fn interpol_empty() {
        assert_eq!(interpol(100.0, vec![], vec![], None), 0.0);
    }

    #[test]
    fn interpol_without_spline() {
        let mjds = vec![0.0, 1.0];
        let duts = vec![0.1, 0.2];
        let v = interpol(0.5, mjds, duts, None);
        assert!((v - 0.15).abs() < 1e-9, "linear interp={}", v);
    }

    fn load_finals_file() -> Option<(Vec<f64>, Vec<f64>, Option<Vec<f64>>)> {
        let data = std::fs::read("finals.all").ok()?;
        let (mjds, duts) = load_finals(&data).ok()?;
        let sd = build_spline(mjds.clone(), duts.clone());
        Some((mjds, duts, sd))
    }

    fn double_to_ns(val: f64) -> i64 {
        (val * 1e9) as i64
    }

    #[test]
    fn its_elapsed_ns_epoch() {
        let Some((mjds, duts, sd)) = load_finals_file() else {
            eprintln!("skipped: finals.all not found");
            return;
        };
        let off = cumpute_offset(&mjds, &duts, sd.clone());
        let dut1_epoch = interpol(EPOCH_UNIX / SECSPERDAY as f64 + 40587.0, mjds.clone(), duts.clone(), sd.clone());
        let epoch_ns = double_to_ns(EPOCH_UNIX);
        let e_ns = epoch_ns + double_to_ns(off) - double_to_ns(dut1_epoch);
        assert_eq!(its_elapsed_ns(e_ns as u64, off, mjds, duts, sd), 0);
    }

    #[test]
    fn its_elapsed_ns_one_second() {
        let Some((mjds, duts, sd)) = load_finals_file() else {
            eprintln!("skipped: finals.all not found");
            return;
        };
        let off = cumpute_offset(&mjds, &duts, sd.clone());
        let dut1_epoch = interpol(EPOCH_UNIX / SECSPERDAY as f64 + 40587.0, mjds.clone(), duts.clone(), sd.clone());
        let epoch_ns = double_to_ns(EPOCH_UNIX);
        let e_ns = epoch_ns + double_to_ns(off) - double_to_ns(dut1_epoch);
        assert_eq!(its_elapsed_ns((e_ns + 1_000_000_000) as u64, off, mjds, duts, sd), 1_000_000_000);
    }

    #[test]
    fn its_elapsed_ns_one_day() {
        let Some((mjds, duts, sd)) = load_finals_file() else {
            eprintln!("skipped: finals.all not found");
            return;
        };
        let off = cumpute_offset(&mjds, &duts, sd.clone());
        let dut1_epoch = interpol(EPOCH_UNIX / SECSPERDAY as f64 + 40587.0, mjds.clone(), duts.clone(), sd.clone());
        let epoch_ns = double_to_ns(EPOCH_UNIX);
        let e_ns = epoch_ns + double_to_ns(off) - double_to_ns(dut1_epoch);
        let its_day_ns: i64 = 86400 * 1_000_000_000;
        assert_eq!(its_elapsed_ns((e_ns + its_day_ns) as u64, off, mjds, duts, sd), its_day_ns as u64);
    }

    #[test]
    fn its_elapsed_ns_day_spacing() {
        let Some((mjds, duts, sd)) = load_finals_file() else {
            eprintln!("skipped: finals.all not found");
            return;
        };
        let off = cumpute_offset(&mjds, &duts, sd.clone());
        let dut1_epoch = interpol(EPOCH_UNIX / SECSPERDAY as f64 + 40587.0, mjds.clone(), duts.clone(), sd.clone());
        let epoch_ns = double_to_ns(EPOCH_UNIX);
        let e_ns = epoch_ns + double_to_ns(off) - double_to_ns(dut1_epoch);
        let its_day_ns: i64 = 86400 * 1_000_000_000;
        for k in -50i64..150 {
            let base = e_ns + k * its_day_ns / 10;
            let e1 = its_elapsed_ns(base as u64, off, mjds.clone(), duts.clone(), sd.clone());
            let e2 = its_elapsed_ns((base + its_day_ns) as u64, off, mjds.clone(), duts.clone(), sd.clone());
            let diff = (e2 as i64 - e1 as i64).abs();
            assert_eq!(diff, its_day_ns,
                "86400s apart must differ by 1 ITS day (k={})", k);
        }
    }

    #[test]
    fn load_finals_veclen() {
        let data = std::fs::read("finals.all").unwrap();
        let (mjds, duts) = load_finals(&data).unwrap();
        let now_days = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_secs() / SECSPERDAY;
        let veclen = now_days - 1096;
        assert!(mjds.len() > 0, "loaded 0 entries");
        assert!(mjds.len() as u64 <= veclen + 1000,
            "loaded {} entries, veclen estimate was {}", mjds.len(), veclen);
        assert_eq!(mjds.len(), duts.len());
    }

    #[test]
    fn load_finals_data_sane() {
        let data = std::fs::read("finals.all").unwrap();
        let (mjds, duts) = load_finals(&data).unwrap();
        assert!(mjds.len() > 10000, "expected >10k entries, got {}", mjds.len());
        for (i, (&m, &d)) in mjds.iter().zip(duts.iter()).enumerate() {
            assert!(m > 40000.0 && m < 70000.0, "mjd[{}]={}", i, m);
            assert!(d > -10.0 && d < 10.0, "dut1[{}]={}", i, d);
        }
        for w in mjds.windows(2) {
            assert!(w[1] > w[0], "mjds not sorted: {} >= {}", w[0], w[1]);
        }
    }

    #[test]
    fn cumpute_offset_golden() {
        let Some((mjds, duts, sd)) = load_finals_file() else {
            eprintln!("skipped: finals.all not found");
            return;
        };
        let off = cumpute_offset(&mjds, &duts, sd);
        assert!(off >= 44193.5 && off <= 44194.0, "offset={}", off);
    }
}
