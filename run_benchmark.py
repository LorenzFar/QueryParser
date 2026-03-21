import duckdb
import time

SCALE_FACTORS = [
    ("sf0.5", "./sf0.5/"),
    ("sf1",   "./sf1/"),
    ("sf2",   "./sf2/"),
    ("sf5",   "./sf5/"),
]

WARMUP_RUNS = 3
TIMED_RUNS  = 10

QUERY = """
SELECT
    SUM(l_extendedprice * (1 - l_discount)) AS revenue
FROM
    read_parquet('{lineitem}lineitem.parquet'),
    read_parquet('{part}part.parquet')
WHERE
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#22'
		and p_container in ('SM CASE', 'SM BOX', 'SM PACK', 'SM PKG')
		and l_quantity >= 8 and l_quantity <= 8 + 10
		and p_size between 1 and 5
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	)
	or
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#23'
		and p_container in ('MED BAG', 'MED BOX', 'MED PKG', 'MED PACK')
		and l_quantity >= 10 and l_quantity <= 10 + 10
		and p_size between 1 and 10
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	)
	or
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#12'
		and p_container in ('LG CASE', 'LG BOX', 'LG PACK', 'LG PKG')
		and l_quantity >= 24 and l_quantity <= 24 + 10
		and p_size between 1 and 15
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	);
"""

def run_benchmark(sf_label, sf_path):
    con = duckdb.connect()
    con.execute("PRAGMA threads=1")

    query = QUERY.format(lineitem=sf_path, part=sf_path)

    print(f"\nBenchmarking SF {sf_label} ({WARMUP_RUNS} warmup + {TIMED_RUNS} timed runs)...")

    timed_ms  = []
    revenue   = None

    for run in range(WARMUP_RUNS + TIMED_RUNS):
        t_start = time.perf_counter()
        result  = con.execute(query).fetchone()
        t_end   = time.perf_counter()

        elapsed_ms = (t_end - t_start) * 1000

        if run >= WARMUP_RUNS:
            timed_ms.append(elapsed_ms)
            revenue = result[0]
            #print(f"  run {run - WARMUP_RUNS + 1:2d}/{TIMED_RUNS}  ({elapsed_ms:.1f} ms)")

    con.close()

    avg_ms  = sum(timed_ms) / len(timed_ms)
    min_ms  = min(timed_ms)
    max_ms  = max(timed_ms)

    print(f"\n╔══════════════════════════════════════════════╗")
    print(f"║  Scale Factor: {sf_label:<29} ║")
    print(f"╠══════════════════════════════════════════════╣")
    print(f"║  Revenue:      {revenue:>20.4f}          ║")
    print(f"╠══════════════════════════════════════════════╣")
    print(f"║  Avg over 10 runs (runs 4-13)                ║")
    print(f"╠══════════════════════════════════════════════╣")
    print(f"║  Avg:          {avg_ms/1000:>14.4f} s              ║")
    print(f"║  Min:          {min_ms/1000:>14.4f} s              ║")
    print(f"║  Max:          {max_ms/1000:>14.4f} s              ║")
    print(f"╚══════════════════════════════════════════════╝")

    return avg_ms, revenue

if __name__ == "__main__":
    print("=" * 48)
    print("  DuckDB TPC-H Q19 Benchmark (single thread)")
    print("=" * 48)

    summary = []
    for sf_label, sf_path in SCALE_FACTORS:
        avg_ms, revenue = run_benchmark(sf_label, sf_path)
        summary.append((sf_label, avg_ms, revenue))

    print("\n╔══════════════════════════════════════════════╗")
    print("║               Summary                        ║")
    print("╠══════════╦══════════════╦════════════════════╣")
    print("║  SF      ║  Avg (ms)    ║  Revenue           ║")
    print("╠══════════╬══════════════╬════════════════════╣")
    for sf_label, avg_ms, revenue in summary:
        print(f"║  {sf_label:<8}║  {avg_ms/1000:>10.4f}  ║  {revenue:>16.4f}  ║")
    print("╚══════════╩══════════════╩════════════════════╝")