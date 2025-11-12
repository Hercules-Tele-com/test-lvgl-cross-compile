# EMBOO EV Dashboard - Manual SquareLine Build Guide

Since the `.spj` file is too large to transfer, follow these steps to recreate it manually in SquareLine Studio.

## 🎯 Setup New Project

1. **Open SquareLine Studio 1.5.4**
2. **Create New Project:**
   - Template: **Blank**
   - Resolution: **800 x 480**
   - Color depth: **16 bit**
   - LVGL version: **8.3.11**
   - Project name: **lvgl-demo**
   - Save location: Your project folder

3. **Configure Export Path:**
   - File → Project Settings
   - UI Files Export Path: `C:\Users\Mike\Repositories\leaf_cruiser\test-lvgl-cross-compile\ui-dashboard\src\ui`

## 📐 Layout Structure

Create this hierarchy (I'll guide you widget by widget):

```
ScreenMain (SCREEN - 800x480, bg: #111827)
├── TopBar (OBJECT - 800x70, flex row, bg: #1e293b)
│   ├── ContainerSOC (OBJECT - 150x60, flex col)
│   │   ├── LabelSOC (LABEL - "100%", font: montserrat_32, color: emerald)
│   │   └── LabelRange (LABEL - "245 km", font: montserrat_14, gray)
│   ├── ContainerLogo (OBJECT - 150x60, flex col)
│   │   ├── ImageLogo (IMAGE - 50x50, assets/EMBOO_LogoSunsetOrange.png)
│   │   └── LabelPowered (LABEL - "POWERED BY EMBOO", font: 10)
│   └── ContainerTime (OBJECT - 150x60, flex col)
│       ├── LabelTime (LABEL - "14:23", font: montserrat_28)
│       └── LabelTimeCaption (LABEL - "LOCAL TIME", font: 10)
├── MainArea (OBJECT - 800x280, no layout, transparent)
│   ├── LabelPRNDL (LABEL - 100x100, "D", left side, orange border)
│   ├── ArcTorque (ARC - 280x280, center, range: -100 to 320)
│   ├── ContainerSpeed (OBJECT - center, flex col)
│   │   ├── LabelSpeed (LABEL - "85", font: montserrat_48)
│   │   ├── LabelSpeedUnit (LABEL - "km/h", font: 20)
│   │   ├── LabelTorque (LABEL - "150", font: 24, orange)
│   │   └── LabelTorqueUnit (LABEL - "Nm", font: 14)
│   └── ContainerPower (OBJECT - 600x60, bottom, power bars)
│       └── (Power bar structure - see below)
├── OdoSection (OBJECT - 800x40, flex row)
│   ├── OdoTotalPanel (flex col)
│   │   ├── LabelOdoTotalCap (LABEL - "TOTAL")
│   │   └── LabelOdoTotal (LABEL - "12847.3 km")
│   ├── OdoDivider (OBJECT - 1px wide line)
│   └── OdoTripPanel (flex col)
│       ├── LabelOdoTripCap (LABEL - "TRIP")
│       └── LabelOdoTrip (LABEL - "156.8 km", orange)
└── TempSection (OBJECT - 800x90, flex col, 3 rows)
    ├── TempMotorRow → Label + Bar (20-110°C) + Temp label
    ├── TempInverterRow → Label + Bar (20-70°C) + Temp label
    └── TempBatteryRow → Label + Bar (20-50°C) + Temp label
```

## 🔨 Step-by-Step Widget Creation

### 1. Top Bar

**Create TopBar container:**
1. Add Object (container)
2. Name: `TopBar`
3. Position: X=0, Y=0
4. Size: W=800, H=70
5. Layout: **Flex Row**, Main Alignment = Space Between
6. Background: Color=#1e293b (slate-800), Opacity=80%
7. Border: Bottom only, color=#f97316 (orange), width=1, opacity=30%
8. Padding: Left=20, Right=20

**Add SOC Container (left):**
1. Add Object inside TopBar
2. Name: `ContainerSOC`
3. Size: W=150, H=60
4. Layout: **Flex Column**, Main Alignment = Start
5. Background: Transparent (opacity=0)
6. Padding Row: 2px

**Add LabelSOC:**
1. Add Label inside ContainerSOC
2. Name: `LabelSOC`
3. Text: "100%"
4. Font: montserrat_32
5. Color: #10b981 (emerald-400)
6. Size: Content (1x1)

**Add LabelRange:**
1. Add Label inside ContainerSOC
2. Name: `LabelRange`
3. Text: "245 km"
4. Font: montserrat_14
5. Color: #94a3b8 (slate-400)

**Repeat for Logo and Time containers** (follow same pattern with their child labels)

### 2. PRNDL Gear Indicator

1. Add Label in MainArea
2. Name: `LabelPRNDL`
3. Position: X=50, Y=90 (from left)
4. Align: LEFT_MID
5. Size: W=100, H=100
6. Text: "D"
7. Font: montserrat_48
8. Text Color: #f97316 (orange)
9. Text Align: CENTER
10. Background: #0f172a (very dark), Opacity=100%
11. Border: Color=#f97316, Width=3
12. Radius: 16px

### 3. Torque Arc

1. Add **Arc** widget in MainArea
2. Name: `ArcTorque`
3. Position: X=0, Y=0
4. Align: CENTER
5. Size: W=280, H=280
6. **Range: Min=-100, Max=320** ← CRITICAL
7. Value: 150 (default)
8. **Background angles: Start=135°, End=45°** (270° arc)
9. Mode: NORMAL
10. Main part (background arc):
    - Arc Color: #334155 (slate), Opacity=30%
    - Arc Width: 20px
11. Indicator part (active arc):
    - Arc Color: #f97316 (orange)
    - Arc Width: 20px
12. Knob: Opacity=0 (hide the knob)

### 4. Speed Display (Center)

1. Add Object in MainArea
2. Name: `ContainerSpeed`
3. Position: X=0, Y=-20
4. Align: CENTER
5. Size: W=200, H=150
6. Layout: **Flex Column**
7. Background: Transparent

**Add child labels:**
- `LabelSpeed`: "85", font=48, white
- `LabelSpeedUnit`: "km/h", font=20, gray
- `LabelTorque`: "150", font=24, orange
- `LabelTorqueUnit`: "Nm", font=14, gray

### 5. Power Bar (Bidirectional)

This is the tricky part - we need TWO bars overlapping:

1. **Create container:**
   - Name: `ContainerPower`
   - Position: X=0, Y=-20, Align=BOTTOM_MID
   - Size: W=600, H=60

2. **Create label row** (top of container):
   - Container with 3 labels: "REGEN" (blue), "45 kW" (center), "POWER" (orange)

3. **Create power bar background:**
   - Add Object, Name: `PowerBarBG`
   - Size: W=600, H=24
   - Background: #1e293b, opacity=50%
   - Border: #475569, width=1
   - Radius: 12px

4. **Add center line** (inside PowerBarBG):
   - Add Object, Name: `CenterLine`
   - Size: W=2, H=24
   - Align: CENTER
   - Background: #64748b (gray)
   - No border

5. **Add BarRegen** (inside PowerBarBG):
   - Widget: **BAR**
   - Name: `BarRegen`
   - Position: Align=RIGHT_MID
   - Size: W=300, H=24
   - Range: 0 to 100
   - Value: 0 (default hidden)
   - Main part: Transparent
   - **Indicator: Color=#60a5fa (blue), Radius=12px**

6. **Add BarPower** (inside PowerBarBG):
   - Widget: **BAR**
   - Name: `BarPower`
   - Position: Align=LEFT_MID
   - Size: W=300, H=24
   - Range: 0 to 100
   - Value: 45
   - Main part: Transparent
   - **Indicator: Color=#f97316 (orange), Radius=12px**

### 6. Odometer Section

1. Create Object: `OdoSection`
2. Position: Y=350
3. Size: W=800, H=40
4. Layout: **Flex Row**, Main Alignment=Center, Gap=40px
5. Background: #1e293b, opacity=60%
6. Border: Top only, #475569

**Add two panels (Total and Trip):**
- Each is a Flex Column container
- Top label: "TOTAL" or "TRIP" (font=10, gray)
- Bottom label: "12847.3 km" or "156.8 km" (font=20, white/orange)
- Divider between them: 1px wide Object, height=32px, gray

### 7. Temperature Section

1. Create Object: `TempSection`
2. Position: Y=390
3. Size: W=800, H=90
4. Layout: **Flex Column**, Gap=8px
5. Background: #1e293b, opacity=80%
6. Border: Top only, orange
7. Padding: 12px all sides

**Create 3 identical rows** (Motor, Inverter, Battery):

For each row:
1. Add Object container (W=760, H=20, Flex Row, Space Between)
2. Add Label: "MOTOR" / "INVERTER" / "BATTERY" (W=100)
3. Add **BAR widget**: 
   - Size: W=500, H=12
   - **Motor: Range=20 to 110, Value=65**
   - **Inverter: Range=20 to 70, Value=48**
   - **Battery: Range=20 to 50, Value=32**
   - Main: #0f172a (dark background), Radius=6px
   - Indicator: #10b981 (green), Radius=6px
4. Add Label: "65°C" / "48°C" / "32°C" (W=80, align right)

## 🎨 Color Reference

```
Orange (EMBOO primary): #f97316 (249, 115, 22)
Blue (regen): #60a5fa (96, 165, 250)
Emerald (good): #10b981 (16, 185, 129)
Yellow (warning): #fbbf24 (251, 191, 36)
Red (critical): #f87171 (248, 113, 113)
White: #ffffff
Slate-900: #0f172a
Slate-800: #1e293b
Slate-700: #334155
Slate-500: #64748b
Slate-400: #94a3b8
```

## ✅ Verification Checklist

After building, verify:
- [ ] All widget names match (LabelSOC, ArcTorque, BarPower, etc.)
- [ ] Arc range is -100 to 320 (not 0-100!)
- [ ] Temperature bar ranges are correct (20-110, 20-70, 20-50)
- [ ] Two power bars exist (BarRegen and BarPower)
- [ ] Fonts are correct sizes (10, 14, 20, 24, 28, 32, 48)
- [ ] Logo image path is correct
- [ ] Export path is configured

## 💾 Export

1. **Export UI Files**: Export → Export UI Files
2. This generates `ui/` folder with all C code
3. Ready to integrate with your CAN data!

---

**Estimated build time:** 45-60 minutes if following carefully

**Pro tip:** Build one section at a time, test alignment, then move to next section.