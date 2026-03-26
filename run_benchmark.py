import duckdb
import time
import csv

SCALE_FACTORS = [
    ("0.5", "./sf0.5/"),
    ("1",   "./sf1/"),
    ("2",   "./sf2/"),
    ("5",   "./sf5/"),
    ("10",   "./sf10/"),
    ("20", "./sf20/")
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

    timed_s = []
    revenue  = None
    for run in range(WARMUP_RUNS + TIMED_RUNS):
        t_start = time.perf_counter()
        result  = con.execute(query).fetchone()
        t_end   = time.perf_counter()
        if run >= WARMUP_RUNS:
            timed_s.append(t_end - t_start)
            revenue = result[0]
    con.close()

    avg_s = sum(timed_s) / len(timed_s)
    print(f"Completed DuckDB SF {sf_label}")
    return avg_s, revenue

if __name__ == "__main__":
    # read existing CSV rows written by C++
    rows = []
    with open("results.csv", "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)

    # run duckdb and merge
    duckdb_results = {}
    for sf_label, sf_path in SCALE_FACTORS:
        avg_s, revenue = run_benchmark(sf_label, sf_path)
        duckdb_results[sf_label] = (avg_s, revenue)

    # rewrite CSV with all columns filled
    with open("results.csv", "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "SF",
            "Average Time (Optimised)",
            "Average Time (DuckDB)",
            "Revenue (Optimised)",
            "Revenue (DuckDB)",
            "Time Improvement",
            "Difference"
        ])
        for row in rows:
            sf         = row["SF"]
            opt_time   = float(row["Average Time (Optimised)"])
            opt_rev    = float(row["Revenue (Optimised)"])
            duck_time, duck_rev = duckdb_results[sf]
            improvement = duck_time - opt_time
            difference  = (duck_time - opt_time) / duck_time * 100
            writer.writerow([
                sf,
                f"{opt_time:.4f}",
                f"{duck_time:.4f}",
                f"{opt_rev:.4f}",
                f"{duck_rev:.4f}",
                f"{improvement:.4f}",
                f"{difference:.2f}%"
            ])

    print("\nResults written to results.csv")