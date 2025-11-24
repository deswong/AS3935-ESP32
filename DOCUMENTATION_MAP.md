# Documentation Map

Quick guide to finding what you need.

## For Different Users

### 🚀 First-Time Users
Start here → [GETTING_STARTED.md](GETTING_STARTED.md)
- Hardware setup
- Wi-Fi provisioning
- MQTT configuration
- Verification steps

### ⚡ Quick Reference
Need something fast? → [CHEAT_SHEET.md](CHEAT_SHEET.md)
- Pin configurations
- Common REST API calls
- Default settings
- Troubleshooting quick hits

### 🔌 Hardware Setup
Building the device? → [HARDWARE.md](HARDWARE.md)
- Bill of Materials (BOM)
- Wiring schematic
- Assembly instructions
- I2C troubleshooting

### 🌐 REST API
Building an integration? → [API_REFERENCE.md](API_REFERENCE.md)
- All 20+ endpoints documented
- Request/response examples
- Error handling
- Complete workflows

### 📖 Overview
Understanding the project? → [README.md](README.md)
- Features list
- Architecture
- Security notes
- Troubleshooting

### 🚢 Production Deployment
Ready to release? → [RELEASE.md](RELEASE.md)
- Pre-release checklist
- Verification procedures
- Known limitations
- Security recommendations

## File Organization

```
/root/
├── README.md              (Start here for overview)
├── GETTING_STARTED.md     (First-time setup, 10 min read)
├── CHEAT_SHEET.md         (Quick reference, 2 min read)
├── API_REFERENCE.md       (REST endpoints, 15 min reference)
├── HARDWARE.md            (Bill of materials & schematic, 20 min)
├── RELEASE.md             (Deployment checklist, 10 min)
├── DOCUMENTATION_MAP.md   (This file)
│
└── docs/archive/          (Old development notes, reference only)
    ├── ADVANCED_SETTINGS_INFO_UPDATES.md
    ├── AF_DISPLAY_FIX.md
    ├── ... (49 old files)
    └── (Historical development records)
```

## Document Sizes

- **README.md** (12 KB) - Main overview and features
- **GETTING_STARTED.md** (8 KB) - Setup guide with step-by-step instructions
- **API_REFERENCE.md** (16 KB) - Complete REST API documentation
- **HARDWARE.md** (12 KB) - Hardware setup and BOM
- **CHEAT_SHEET.md** (4 KB) - Quick reference
- **RELEASE.md** (4 KB) - Deployment checklist

**Total**: ~56 KB of production documentation

## Reading Time Estimates

- **GETTING_STARTED.md**: 10-15 minutes
- **CHEAT_SHEET.md**: 2-5 minutes (reference)
- **API_REFERENCE.md**: 20-30 minutes (reference, not sequential)
- **HARDWARE.md**: 20-30 minutes (if building hardware)
- **README.md**: 5-10 minutes
- **RELEASE.md**: 5-10 minutes

## Common Tasks

| Task | Go To |
|------|-------|
| Build and flash the device | [GETTING_STARTED.md](GETTING_STARTED.md) |
| Set up Wi-Fi | [GETTING_STARTED.md](GETTING_STARTED.md) - Step 4 |
| Configure MQTT | [GETTING_STARTED.md](GETTING_STARTED.md) - Step 4 |
| Understand all REST endpoints | [API_REFERENCE.md](API_REFERENCE.md) |
| Wire the hardware | [HARDWARE.md](HARDWARE.md) - Wiring Schematic |
| Find the BOM | [HARDWARE.md](HARDWARE.md) - Bill of Materials |
| Get quick commands | [CHEAT_SHEET.md](CHEAT_SHEET.md) |
| Understand features | [README.md](README.md) - Features & Configuration |
| Deploy to production | [RELEASE.md](RELEASE.md) |
| Troubleshoot an issue | [README.md](README.md) - Troubleshooting section |
| Debug I2C problems | [HARDWARE.md](HARDWARE.md) - Troubleshooting Hardware |

## Documentation Status

✅ **Complete Production Documentation**

- [x] Core feature documentation (README.md)
- [x] First-time setup guide (GETTING_STARTED.md)
- [x] Quick reference card (CHEAT_SHEET.md)
- [x] Complete REST API reference (API_REFERENCE.md)
- [x] Hardware setup & BOM (HARDWARE.md)
- [x] Release deployment checklist (RELEASE.md)
- [x] Old development docs archived (docs/archive/)
- [x] .gitignore configured to exclude archive
- [x] All documentation reviewed for accuracy

## Version

- **Documentation Version**: 1.0.0
- **Last Updated**: November 2025
- **Status**: Production-ready

## Support

- **Issues**: Open a GitHub issue
- **Questions**: Check the relevant documentation from the map above
- **Contributions**: Submit pull requests with clear descriptions
