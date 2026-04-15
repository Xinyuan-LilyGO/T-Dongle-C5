# ESP32-C5 Project Documentation

This repository contains the latest technical specifications and directory structures for the ESP32-C5 project.

---

## Recent Updates

The following documentation and structural updates have been applied to align with the latest official hardware releases.

---

## Documentation Upgrades

- **Technical Reference Manual**
  - Upgraded from *Pre-release v0.1* → **ESP32-C5 TRM Version 1.0**

- **Datasheets**
  - Replaced *v0.5 Pre-release* files with **Datasheet Version 1.1**

---

## Maintenance and Refinement

- **Directory Cleanup**
  - Corrected spelling errors and typos in directory names
  - Ensured consistency with CLI paths

- **File Replacement**
  - Removed deprecated pre-release files
  - Replaced with finalized manufacturer documentation

---

## Repository Structure
- Structure:
  - /docs # Latest TRM and Datasheets
  - /src # Hardware abstraction layers and sample code
  - /resources # Reference diagrams and pinout maps
  - /Hardware # Scimatics
  - /Firmware # Firmware files for base system
  - /arduino # Example code for Arduino
  - /idf # Example code for ESP-IDF




## Usage

> **Important Notice**

Please ensure you are referencing **TRM Version 1.0** for all register-level development.

Several bit mappings have changed since the **v0.1 pre-release**, and using outdated documentation may result in incorrect implementation.



## Notes

- Always verify documentation versions before development.
- Keep repository structure consistent when adding new files.
- Avoid using deprecated pre-release materials.

---
