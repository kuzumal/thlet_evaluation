import numpy as np
import re

def filter_outliers(data):
  q1 = np.percentile(data, 25)
  q3 = np.percentile(data, 75)
  iqr = q3 - q1
  lower_bound = q1 - 1.5 * iqr
  upper_bound = q3 + 1.5 * iqr
  return [x for x in data if lower_bound <= x <= upper_bound]

logs = []
with open('result.txt', 'r') as f:
  for line in f:
    nums = list(map(int, re.findall(r'\d+', line)))
    if nums:
      logs.append(nums)

matrix = np.array(logs)
for i in range(matrix.shape[1]):
  clean_column = filter_outliers(matrix[:, i])
  print(f"Column {i+1} Avg: {np.mean(clean_column):.2f} (Filtered {len(matrix)-len(clean_column)} outliers)")