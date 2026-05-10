# Plan: <TASK-ID>

## Overview
<!-- Brief summary of the implementation approach -->

## Steps

### Step 1: <title>
- **File**: `software/fish-arm/...`
- **Action**: add / modify / remove
- **Details**: <!-- code sketch, data shapes, function signatures -->
- **Verify**: <!-- how to confirm this step worked -->

### Step 2: <title>
- **File**: `software/fish-arm/...`
- **Action**:
- **Details**:
- **Verify**:

<!-- Add more steps as needed -->

## Build / Test Commands
```bash
# Backend
cd software/fish-arm/backend-spring && mvn spring-boot:run

# Frontend
cd software/fish-arm/front-vue && npm run dev

# Bridge
cd software/fish-arm && python serial_bridge.py --port COM20
```
