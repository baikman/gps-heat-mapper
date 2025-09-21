# GPS Heatmapper

## Summary
Pico 2W retrieves GPS data periodically, creates a heatmap, and hosts a website on the Pico 2W for access.

---

## Hardware
- Pico 2W  
- NEO6MV2  
- SSD1306
- LiPo + Charger (?)

### Notes
- Use **NMEA protocol** for GPS info.  
- Handle different states:  
  - Connected to satellite  
  - Attempting to connect  
  - Not connected  
- Indicate state status.  

---

## To Do
- [ ] Set up a working GPS logger on Pico that outputs to the SSD1306.  
- [ ] Configure Pico as an access point to display GPS logs on a webpage.  
- [ ] Make the setup portable (optional).  
- [ ] Implement heatmap.  
- [ ] Add hourly ping and unit-by-unit squares.  
- [ ] Explore power management options.  