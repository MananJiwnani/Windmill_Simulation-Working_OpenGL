# Windmill Simulation using OpenGL

This project simulates a windmill whose rotation responds dynamically to wind speed and direction using a simple physics-based model.

---

## 🎮 Controls

| Input | Action |
|------|--------|
| ⬆️ Up Arrow | Increase absolute wind speed |
| ⬇️ Down Arrow | Decrease absolute wind speed |
| ⬅️ Left Arrow | Rotate scene left |
| ➡️ Right Arrow | Rotate scene right |
| A | Change wind direction (left) |
| D | Change wind direction (right) |
| Mouse Scroll Up | Zoom in |
| Mouse Scroll Down | Zoom out |
| R | Toggle random wind mode |

---

## ⚙️ Rotation Model (Short Explanation)

The windmill rotation is governed by a simple balance between wind force and resistance:

acc = progstep*cos(wind_y)*wind_acc_factor - wing_speed*turbine_factor

### Key Idea

- The **first term** represents wind pushing the blades  
- The **second term** represents resistance (drag) opposing motion  

So:
### Steady Speed

As the blades spin faster:
- Resistance increases  
- Eventually, resistance balances wind force  

At this point:
The windmill reaches a **constant rotation speed**, similar to terminal velocity in physics.

