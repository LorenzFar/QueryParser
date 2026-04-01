import sys
import os
import csv
import duckdb
import time

WARMUP_RUNS = 3
TIMED_RUNS  = 10

QUERY = """
SELECT
    SUM(l_extendedprice * (1 - l_discount)) AS revenue
FROM
    read_parquet('{lineitem}'),
    read_parquet('{part}')
WHERE
    (
        p_partkey = l_partkey
        and p_brand = 'Brand#22'
        and p_container in ('SM CASE', 'SM BOX', 'SM PACK', 'SM PKG')
        and l_quantity >= 8 and l_quantity <= 18
        and p_size between 1 and 5
        and l_shipmode in ('AIR', 'AIR REG')
        and l_shipinstruct = 'DELIVER IN PERSON'
    )
    or
    (
        p_partkey = l_partkey
        and p_brand = 'Brand#23'
        and p_container in ('MED BAG', 'MED BOX', 'MED PKG', 'MED PACK')
        and l_quantity >= 10 and l_quantity <= 20
        and p_size between 1 and 10
        and l_shipmode in ('AIR', 'AIR REG')
        and l_shipinstruct = 'DELIVER IN PERSON'
    )
    or
    (
        p_partkey = l_partkey
        and p_brand = 'Brand#12'
        and p_container in ('LG CASE', 'LG BOX', 'LG PACK', 'LG PKG')
        and l_quantity >= 24 and l_quantity <= 34
        and p_size between 1 and 15
        and l_shipmode in ('AIR', 'AIR REG')
        and l_shipinstruct = 'DELIVER IN PERSON'
    );
"""

def run_benchmark(sf_path):
    con = duckdb.connect()
    con.execute("PRAGMA threads=1")

    lineitem_file = os.path.join(sf_path, "lineitem.parquet")
    part_file     = os.path.join(sf_path, "part.parquet")

    query = QUERY.format(lineitem=lineitem_file, part=part_file)

    timed_s = []
    result = None
    for run in range(WARMUP_RUNS + TIMED_RUNS):
        t_start = time.time()
        result = con.execute(query).fetchone()
        t_end = time.time()
        if run >= WARMUP_RUNS:
            timed_s.append(t_end - t_start)

    con.close()

    avg_s = sum(timed_s) / len(timed_s)
    return avg_s, result[0]

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python run_benchmark.py <sf_path>")
        sys.exit(1)

    sf_path = sys.argv[1]
    csv_name = sys.argv[2]

    rows = []
    with open(csv_name, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)

    duck_time, duck_revenue = run_benchmark(sf_path)

    with open(csv_name, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["Result (Optimised)", "Result (DuckDB)"])
        for row in rows:
            opt_rev  = float(row["Result (Optimised)"])
            writer.writerow([f"{opt_rev:.4f}", f"{duck_revenue:.4f}"])

    # Print only the average time as last line
    print(f"{duck_time:.4f}")