#!/usr/bin/env python3
"""
Generate moon phase XBM bitmaps for OLED display
Creates 8 moon phases: New Moon, Waxing Crescent, First Quarter, Waxing Gibbous,
                      Full Moon, Waning Gibbous, Third Quarter, Waning Crescent
"""

def create_moon_phase_bitmap(phase, width=32, height=32):
    """Create a moon phase bitmap based on phase (0-7)"""
    bitmap = [[0 for _ in range(width)] for _ in range(height)]
    center_x, center_y = width // 2, height // 2
    radius = 12  # Moon radius
    
    # For each pixel, determine if it's inside the moon circle
    for y in range(height):
        for x in range(width):
            dx = x - center_x
            dy = y - center_y
            dist_sq = dx * dx + dy * dy
            
            # Check if pixel is inside moon circle
            if dist_sq <= radius * radius:
                # Determine if this pixel should be lit based on phase
                lit = False
                
                if phase == 0:  # New Moon - empty
                    lit = False
                elif phase == 1:  # Waxing Crescent - right side lit
                    lit = (dx > -2)  # Right half with small offset
                elif phase == 2:  # First Quarter - right half lit
                    lit = (dx >= 0)
                elif phase == 3:  # Waxing Gibbous - mostly lit, left edge dark
                    lit = (dx > -4)  # Most of moon lit
                elif phase == 4:  # Full Moon - fully lit
                    lit = True
                elif phase == 5:  # Waning Gibbous - mostly lit, right edge dark
                    lit = (dx < 4)  # Most of moon lit
                elif phase == 6:  # Third Quarter - left half lit
                    lit = (dx <= 0)
                elif phase == 7:  # Waning Crescent - left side lit
                    lit = (dx < 2)  # Left half with small offset
                
                if lit:
                    bitmap[y][x] = 1
    
    return bitmap

def bitmap_to_bytes(bitmap):
    """Convert bitmap array to XBM byte array"""
    width = len(bitmap[0])
    height = len(bitmap)
    bytes_array = []
    
    for y in range(height):
        for x in range(0, width, 8):
            byte = 0
            for bit in range(8):
                if x + bit < width:
                    if bitmap[y][x + bit]:
                        byte |= (1 << (7 - bit))
            bytes_array.append(byte)
    
    return bytes_array

def format_bytes_for_c(bytes_array, max_per_line=12):
    """Format bytes for C array"""
    lines = []
    for i in range(0, len(bytes_array), max_per_line):
        chunk = bytes_array[i:i+max_per_line]
        hex_str = ', '.join(f'0x{b:02x}' for b in chunk)
        lines.append(f'  {hex_str}')
    return ',\n'.join(lines)

# Generate all 8 moon phases
phases = []
for phase in range(8):
    bitmap = create_moon_phase_bitmap(phase)
    bytes_array = bitmap_to_bytes(bitmap)
    phases.append((phase, bytes_array))

# Generate C header file
header = """// Moon Phase XBM Bitmaps for OLED Display
// 8 phases: New Moon, Waxing Crescent, First Quarter, Waxing Gibbous,
//           Full Moon, Waning Gibbous, Third Quarter, Waning Crescent
// Each bitmap is 32x32 pixels (128 bytes = 32*32/8)

#define moon_phase_width 32
#define moon_phase_height 32

"""

phase_names = [
    "New Moon",
    "Waxing Crescent",
    "First Quarter",
    "Waxing Gibbous",
    "Full Moon",
    "Waning Gibbous",
    "Third Quarter",
    "Waning Crescent"
]

for phase, bytes_array in phases:
    name = phase_names[phase]
    header += f"// {name} (Phase {phase})\n"
    header += f"static const unsigned char moon_phase_{phase}_bits[] = {{\n"
    header += format_bytes_for_c(bytes_array)
    header += "\n};\n\n"

header += "// Array of moon phase bitmaps for easy access\n"
header += "static const unsigned char* moon_phases[] = {\n"
for phase in range(8):
    header += f"  moon_phase_{phase}_bits,  // {phase_names[phase]}\n"
header += "};\n"

# Write to file
with open('Images/moonPhases.h', 'w') as f:
    f.write(header)

print("Moon phase bitmaps generated successfully!")

