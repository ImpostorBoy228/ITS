use arika::earth::eop::finals2000a::Finals2000A;
use hifitime::{Duration, Epoch};
use std::fs;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let content = fs::read_to_string("finals.all")?;
    let entries = Finals2000A::parse(&content)?;

    let latest = entries.last().expect("empty");
    let _mjd = latest.mjd; // Modified Julian Date (double)
    let ut1_minus_utc = latest.dut1;

    let now_utc = Epoch::now().unwrap();

    let offset = Duration::from_seconds(ut1_minus_utc);
    let now_ut1 = now_utc + offset;

    println!("UT1: {:?}", now_ut1);

    Ok(())
}
