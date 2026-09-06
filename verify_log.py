import re
import csv

LOG_FILE = "uart_log.txt"
RESULT_FILE = "test_results.csv"


def load_log():
    with open(LOG_FILE, "r", encoding="utf-8") as f:
        return f.readlines()


def parse_current(line):
    pattern = (
        r"Raw=([-\d.]+).*"
        r"Filt=([-\d.]+).*"
        r"Base=([-\d.]+).*"
        r"Th=([-\d.]+).*"
        r"dI=([-\d.]+).*"
        r"Count=(\d+)"
    )

    match = re.search(pattern, line)

    if not match:
        return None

    return {
        "raw": float(match.group(1)),
        "filtered": float(match.group(2)),
        "baseline": float(match.group(3)),
        "threshold": float(match.group(4)),
        "slope": float(match.group(5)),
        "count": int(match.group(6)),
    }


def find_index(lines, keyword, start=0):
    for i in range(start, len(lines)):
        if keyword in lines[i]:
            return i
    return -1


def main():

    lines = load_log()

    results = []

    # ------------------------------------------------
    # TC-04
    # Baseline Ready 이전 Anti-Pinch 오검출 여부
    # ------------------------------------------------

    manual_up_idx = find_index(lines, "[WINDOW] MANUAL_UP")
    baseline_idx = find_index(lines, "[BASELINE] Ready")

    startup_false_positive = False

    if manual_up_idx != -1 and baseline_idx != -1:

        for line in lines[manual_up_idx:baseline_idx]:
            if "[ANTI-PINCH]" in line:
                startup_false_positive = True

    tc04_pass = (
        manual_up_idx != -1
        and baseline_idx != -1
        and not startup_false_positive
    )

    results.append({
        "TC_ID": "TC-04",
        "Objective": "Startup/learning 구간 Anti-Pinch 오검출 방지",
        "Result": "PASS" if tc04_pass else "FAIL"
    })


    # ------------------------------------------------
    # TC-05
    # 정상 전류 구간에서 Count=0 유지 여부
    # ------------------------------------------------

    normal_samples = 0
    normal_fail = False

    current_data = []

    for line in lines:

        data = parse_current(line)

        if data:
            current_data.append(data)

            if data["filtered"] < data["threshold"]:
                normal_samples += 1

                if data["count"] != 0:
                    normal_fail = True

    tc05_pass = normal_samples > 0 and not normal_fail

    results.append({
        "TC_ID": "TC-05",
        "Objective": "정상 부하에서 Anti-Pinch Count=0 유지",
        "Result": "PASS" if tc05_pass else "FAIL"
    })


    # ------------------------------------------------
    # TC-06
    # Threshold 단일 초과 후 정상 복귀 시 오검출 여부
    # ------------------------------------------------

    single_spike_found = False
    single_spike_safe = False

    for i in range(len(current_data) - 1):

        current = current_data[i]
        next_sample = current_data[i + 1]

        if (
            current["filtered"] >= current["threshold"]
            and next_sample["filtered"] < next_sample["threshold"]
        ):
            single_spike_found = True

            if next_sample["count"] == 0:
                single_spike_safe = True
                break

    if single_spike_found:
        tc06_result = "PASS" if single_spike_safe else "FAIL"
    else:
        tc06_result = "NOT_TESTED"

    results.append({
        "TC_ID": "TC-06",
        "Objective": "단일 Current Spike 내성",
        "Result": tc06_result
    })


    # ------------------------------------------------
    # TC-07
    # Anti-Pinch Event 발생 검증
    # ------------------------------------------------

    anti_event_idx = find_index(
        lines,
        "[ANTI-PINCH] EVENT QUEUED"
    )

    tc07_pass = anti_event_idx != -1

    results.append({
        "TC_ID": "TC-07",
        "Objective": "연속 과전류 Anti-Pinch Event 발생",
        "Result": "PASS" if tc07_pass else "FAIL"
    })


    # ------------------------------------------------
    # TC-08
    # Event 이후 Reverse 상태 전이
    # ------------------------------------------------

    reverse_idx = -1

    if anti_event_idx != -1:
        reverse_idx = find_index(
            lines,
            "[WINDOW] ANTI_PINCH_REVERSE",
            anti_event_idx + 1
        )

    tc08_pass = (
        anti_event_idx != -1
        and reverse_idx != -1
        and reverse_idx > anti_event_idx
    )

    results.append({
        "TC_ID": "TC-08",
        "Objective": "Anti-Pinch Event 이후 Reverse 상태 전이",
        "Result": "PASS" if tc08_pass else "FAIL"
    })


    # ------------------------------------------------
    # TC-09
    # Reverse 이후 IDLE 복귀
    # ------------------------------------------------

    idle_idx = -1

    if reverse_idx != -1:
        idle_idx = find_index(
            lines,
            "[WINDOW] IDLE",
            reverse_idx + 1
        )

    tc09_pass = (
        reverse_idx != -1
        and idle_idx != -1
        and idle_idx > reverse_idx
    )

    results.append({
        "TC_ID": "TC-09",
        "Objective": "Anti-Pinch Reverse 이후 IDLE 복귀",
        "Result": "PASS" if tc09_pass else "FAIL"
    })


    # ------------------------------------------------
    # TC-10
    # Baseline Learning 완료 여부
    # ------------------------------------------------

    baseline_ready = False
    baseline_value = None

    for line in lines:

        match = re.search(
            r"\[BASELINE\] Ready = ([\d.]+)",
            line
        )

        if match:
            baseline_ready = True
            baseline_value = float(match.group(1))
            break

    results.append({
        "TC_ID": "TC-10",
        "Objective": "Baseline Learning 완료",
        "Result": "PASS" if baseline_ready else "FAIL"
    })


    # ------------------------------------------------
    # CSV 저장
    # ------------------------------------------------

    with open(
        RESULT_FILE,
        "w",
        newline="",
        encoding="utf-8-sig"
    ) as f:

        writer = csv.DictWriter(
            f,
            fieldnames=[
                "TC_ID",
                "Objective",
                "Result"
            ]
        )

        writer.writeheader()
        writer.writerows(results)


    # ------------------------------------------------
    # Console Summary
    # ------------------------------------------------

    print()
    print("================================")
    print(" Power Window Verification")
    print("================================")

    for result in results:

        print(
            f"{result['TC_ID']} | "
            f"{result['Result']} | "
            f"{result['Objective']}"
        )

    print("--------------------------------")

    if baseline_value is not None:
        print(
            f"Detected Baseline: "
            f"{baseline_value:.1f} mA"
        )

    print()
    print(f"Report saved → {RESULT_FILE}")


if __name__ == "__main__":
    main()