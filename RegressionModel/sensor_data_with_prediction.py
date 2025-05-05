import csv
import random
import requests
import time
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from datetime import datetime, timedelta
from sklearn.model_selection import train_test_split
from sklearn.linear_model import LinearRegression

# ThingSpeak API details
THINGSPEAK_WRITE_API_KEY = "TVG5Z9W9AL2XQH81"
THINGSPEAK_URL = "https://api.thingspeak.com/update"

# CSV file setup
CSV_FILENAME = "people_count_data.csv"

# Function to initialize or verify CSV file
def initialize_csv():
    try:
        with open(CSV_FILENAME, mode="w", newline="") as file:
            writer = csv.writer(file)
            writer.writerow(["Timestamp", "Ultrasonic Distance (cm)", "Infrared Detected", "Total People Count", "Entry Count", "Exit Count"])
        print(f"✅ CSV file '{CSV_FILENAME}' initialized with headers.")
    except PermissionError:
        print(f"❌ Error: No permission to write to '{CSV_FILENAME}'. Check file access.")
        exit(1)
    except Exception as e:
        print(f"❌ Error initializing CSV: {e}")
        exit(1)

# Simulation parameters
DAYS = 1
TIME_STEP_MINUTES = 30
DURATION_MINUTES = (DAYS * 24 * 60) // TIME_STEP_MINUTES
ENTRANCE_PROBABILITY = 0.6
EXIT_PROBABILITY = 0.4
MAX_PEOPLE = 20

# Initial people count
people_count = 0
start_time = datetime.now()

# Initialize CSV
initialize_csv()

for step in range(DURATION_MINUTES):
    timestamp = start_time + timedelta(minutes=step * TIME_STEP_MINUTES)
    timestamp_str = timestamp.strftime("%Y-%m-%d %H:%M:%S")

    # Simulated sensor data
    ultrasonic_distance = round(random.uniform(10, 100), 2)
    ir_detected = random.choice([0, 1])

    # Entry & Exit logic
    entry_count = 0
    exit_count = 0

    if random.random() < ENTRANCE_PROBABILITY and people_count < MAX_PEOPLE:
        people_count += 1
        entry_count = 1

    if random.random() < EXIT_PROBABILITY and people_count > 0:
        people_count -= 1
        exit_count = 1

    # Save data to CSV file
    try:
        with open(CSV_FILENAME, mode="a", newline="") as file:
            writer = csv.writer(file)
            writer.writerow([timestamp_str, ultrasonic_distance, ir_detected, people_count, entry_count, exit_count])
        print(f"📝 Data saved to CSV at {timestamp_str}")
    except Exception as e:
        print(f"❌ Error saving to CSV at {timestamp_str}: {e}")
        continue

    # Send data to ThingSpeak
    payload = {
        "api_key": THINGSPEAK_WRITE_API_KEY,
        "field1": ultrasonic_distance,
        "field2": ir_detected,
        "field3": people_count,
        "field4": entry_count,
        "field5": exit_count
    }
    
    try:
        response = requests.get(THINGSPEAK_URL, params=payload, timeout=10)
        if response.status_code == 200 and response.text.strip() != "0":
            print(f"✔ Data sent to ThingSpeak at {timestamp_str}, Response: {response.text}")
        else:
            print(f"❌ Failed to send data at {timestamp_str}, Status: {response.status_code}, Error: {response.text}")
    except requests.RequestException as e:
        print(f"❌ Network error at {timestamp_str}: {e}")
    
    time.sleep(16)  # Respect ThingSpeak 15-second rate limit

print("\n✅ Data collection complete! Data saved to 'people_count_data.csv'")

# Load data from CSV for visualization
try:
    df = pd.read_csv(CSV_FILENAME, parse_dates=["Timestamp"])
    if df.empty:
        print("⚠️ Warning: CSV is empty. No data to process.")
        exit(1)
    df.set_index("Timestamp", inplace=True)
    print("✅ CSV data loaded successfully.")
except FileNotFoundError:
    print(f"❌ Error: '{CSV_FILENAME}' not found. Ensure data was written.")
    exit(1)
except ValueError as e:
    print(f"❌ Error parsing CSV: {e}. Check column names match headers.")
    exit(1)
except Exception as e:
    print(f"❌ Unexpected error loading CSV: {e}")
    exit(1)

# Verify people count range
try:
    assert df["Total People Count"].between(0, MAX_PEOPLE).all(), "❌ People count out of bounds!"
    print("✅ All people counts are within the range 0–20.")
except AssertionError as e:
    print(e)
    exit(1)
except KeyError:
    print("❌ Error: 'Total People Count' column not found in CSV.")
    exit(1)

# Resample data by day for visualization
try:
    daily_people_count = df["Total People Count"].resample("D").mean()
    daily_distance = df["Ultrasonic Distance (cm)"].resample("D").mean()
    daily_ir_detection = df["Infrared Detected"].resample("D").sum()
    daily_entry = df["Entry Count"].resample("D").sum()
    daily_exit = df["Exit Count"].resample("D").sum()
    print("✅ Data resampled successfully.")
except KeyError as e:
    print(f"❌ Error resampling data: Column {e} not found.")
    exit(1)
except Exception as e:
    print(f"❌ Error during resampling: {e}")
    exit(1)

# Plot data
try:
    fig, axes = plt.subplots(3, 1, figsize=(12, 10), sharex=True)

    axes[0].plot(daily_people_count, marker="o", linestyle="-", color="blue", label="Avg People Count")
    axes[0].set_ylabel("People Count")
    axes[0].set_title("Average People Count Per Day")
    axes[0].legend()
    axes[0].grid()

    axes[1].plot(daily_distance, marker="s", linestyle="--", color="green", label="Avg Distance (cm)")
    axes[1].set_ylabel("Distance (cm)")
    axes[1].set_title("Average Ultrasonic Distance Per Day")
    axes[1].legend()
    axes[1].grid()

    axes[2].bar(daily_ir_detection.index, daily_ir_detection, color="red", label="IR Detections")
    axes[2].set_ylabel("IR Detections")
    axes[2].set_title("Total Infrared Detections Per Day")
    axes[2].legend()
    axes[2].grid()

    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.savefig('sensor_data_visualization.png')
    print("✅ Plots saved successfully.")
except Exception as e:
    print(f"❌ Error generating plots: {e}")

# Add time-based features
try:
    df['Hour'] = df.index.hour
    df['DayOfWeek'] = df.index.dayofweek  # Fixed: no parentheses
    print("✅ Time-based features added.")
except Exception as e:
    print(f"❌ Error adding time features: {e}")
    exit(1)

# Define features and target
features = ['Ultrasonic Distance (cm)', 'Infrared Detected', 'Entry Count', 'Exit Count', 'Hour', 'DayOfWeek']
target = 'Total People Count'

try:
    X = df[features]
    y = df[target]
    print("✅ Features and target defined.")
except KeyError as e:
    print(f"❌ Error: Missing feature or target column {e}")
    exit(1)

# Train-test split
try:
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, shuffle=False)
    print("✅ Data split for training.")
except Exception as e:
    print(f"❌ Error splitting data: {e}")
    exit(1)

# Train regression model
try:
    model = LinearRegression()
    model.fit(X_train, y_train)
    print("✅ Model trained successfully.")
except Exception as e:
    print(f"❌ Error training model: {e}")
    exit(1)

# Evaluate the model
try:
    y_pred = model.predict(X_test)
    r2 = r2_score(y_test, y_pred)
    rmse = np.sqrt(mean_squared_error(y_test, y_pred))  # Manual RMSE calculation
    print("\n📊 Regression Model Evaluation:")
    print("R² Score:", r2)
    print("RMSE:", rmse)
    print("✅ Model evaluated.")
except Exception as e:
    print(f"❌ Error evaluating model: {e}")

# Visualize predicted vs actual
try:
    plt.figure(figsize=(10, 4))
    plt.plot(y_test.values, label="Actual", marker='o')
    plt.plot(y_pred, label="Predicted", marker='x')
    plt.title("Actual vs Predicted People Count (Test Set)")
    plt.xlabel("Time Steps")
    plt.ylabel("People Count")
    plt.legend()
    plt.grid()
    plt.tight_layout()
    plt.savefig('actual_vs_predicted.png')
    print("✅ Actual vs Predicted plot saved.")
except Exception as e:
    print(f"❌ Error plotting actual vs predicted: {e}")

# Predict next hour
try:
    future_features = []
    last_row = df.iloc[-1]
    for i in range(12):
        future_time = last_row.name + timedelta(minutes=(i+1) * 5)
        hour = future_time.hour
        day = future_time.dayofweek  # Fixed: attribute, not callable
        ultrasonic_avg = df['Ultrasonic Distance (cm)'].mean()
        ir_prob = df['Infrared Detected'].mean()
        ir_sim = 1 if random.random() < ir_prob else 0
        entry_sim = random.randint(0, 1)
        exit_sim = random.randint(0, 1)
        future_features.append([ultrasonic_avg, ir_sim, entry_sim, exit_sim, hour, day])

    future_predictions = model.predict(future_features)
    print("✅ Future predictions generated.")
except Exception as e:
    print(f"❌ Error generating future predictions: {e}")
    exit(1)

# Send each prediction to ThingSpeak field6
try:
    for i, prediction in enumerate(future_predictions):
        time.sleep(16)  # Respect ThingSpeak 15-second rate limit
        payload = {"api_key": THINGSPEAK_WRITE_API_KEY, "field6": round(prediction, 2)}
        response = requests.get(THINGSPEAK_URL, params=payload, timeout=10)
        if response.status_code == 200 and response.text.strip() != "0":
            print(f"📤 Sent prediction {i+1}/12 to ThingSpeak: {round(prediction, 2)}, Response: {response.text}")
        else:
            print(f"❌ Failed to send prediction {i+1}/12 to ThingSpeak, Status: {response.status_code}, Error: {response.text}")
    print("✅ Predictions sent to ThingSpeak.")
except requests.RequestException as e:
    print(f"❌ Network error sending predictions: {e}")
except Exception as e:
    print(f"❌ Error sending predictions: {e}")

# Plot predictions
try:
    plt.figure(figsize=(10, 4))
    plt.plot(range(1, 13), future_predictions, marker='o', linestyle='-', color='orange')
    plt.title("Predicted People Count for Next Hour")
    plt.xlabel("5-min Intervals")
    plt.ylabel("Predicted People Count")
    plt.grid()
    plt.tight_layout()
    plt.savefig('predicted_people_count.png')
    print("✅ Prediction plot saved.")
except Exception as e:
    print(f"❌ Error plotting predictions: {e}")

print("\n✅ Process completed! Check ThingSpeak for field6 data in ~18 minutes.")