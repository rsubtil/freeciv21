import os
import sys
import re

def print_edit(line, indent, text):
  indent = "\t" * indent
  print(f"{indent}:{line} " + text)

# City will default to producing removed units. Change it to produce "Nothing"
def fix_city_production(lines):
  print("# Fixing city production")
  for i in range(len(lines)):
    if lines[i].startswith("c={\"y\""):
      print_edit(i, 1, "Found city player definition")
      for j in range(i, len(lines)):
        if lines[j].startswith("}"):
          print_edit(j, 1, "Finished city player definition")
          break
        regex = "\"(UnitType|Building)\",\".*?\""
        search = re.search(regex, lines[j])
        if search:
          lines[j] = re.sub(regex, "\"Building\",\"Nothing\"", lines[j])
          print_edit(j, 2, f"{search.group(0)} -> \"Building\",\"Nothing\"")

# Tech will default to Alphabet. Change it to "A_UNSET"
def fix_tech(lines):
  print("# Fixing research")
  for i in range(len(lines)):
    if lines[i].startswith("r={\"number\""):
      print_edit(i, 1, "Found research player definition")
      for j in range(i, len(lines)):
        if lines[j].startswith("}"):
          print_edit(j, 1, "Finished research player definition")
          return
        regex = "\"Alphabet\""
        search = re.search(regex, lines[j])
        if search:
          lines[j] = re.sub(regex, "\"A_UNSET\"", lines[j])
          print_edit(j, 2, f"{search.group(0)} -> \"A_UNSET\"")

# Assign default usernames and border colors to players:
def fix_player_info(lines):
  print("# Fixing player info")
  data = {
  # "player_name": ["username", "color.r", "color.g", "color.b"]],
    "Yellow": ["234", "186", "0"],
    "Blue": ["0", "193", "238"],
    "Green": ["159", "255", "80"],
    "Purple": ["138", "43", "226"],
  }
  color_matches = [
    ("color.r", 0),
    ("color.g", 1),
    ("color.b", 2),
  ]
  for i in range(len(lines)):
    regex = "^name=\".+\""
    search = re.search(regex, lines[i])
    if search:
      curr_name = search.group(0).removeprefix("name=\"").removesuffix("\"")
      print_edit(i, 1, f"Found {curr_name} player definition")

    # Set username
    if lines[i].startswith("username="):
      lines[i] = f"username=\"{curr_name.lower()}\"\n"
      print_edit(i, 2, f"username=\"{curr_name.lower()}\"")

    for match in color_matches:
      regex = f"^{match[0]}=[0-9]+"
      search = re.search(regex, lines[i])
      if search:
        lines[i] = f"{match[0]}={data[curr_name][match[1]]}\n"
        print_edit(i, 2, f"{search.group(0)} -> {match[0]}={data[curr_name][match[1]]}")

# Set some default server settings
def fix_server_settings(lines):
  print("# Fixing server settings")
  data = [
    "\"timeout\",1,1",
    "\"saveturns\",60,60",
    "\"ec_turns\",65535,65535"
  ]
  for i in range(len(lines)):
    if lines[i].startswith("set={\"name\",\"value\",\"gamestart\""):
      print_edit(i, 1, "Found server settings")
      for d in data:
        lines.insert(i + 1, d + "\n")
        print_edit(i + 1, 2, f"Added {d}")
    if lines[i].startswith("set_count="):
      lines[i] = f"set_count={int(lines[i].split('=')[1]) + len(data)}\n"
      return

# Remove existing units that are not in the final scenario
def fix_removed_units(lines):
  print("# Fixing removed units")
  valid_units = [
    "unit_engineer",
    "unit_banker",
    "unit_scientist",
    "Spy"
  ]
  lines_to_erase = []
  for i in range(len(lines)):
    if lines[i].startswith("u={\"id\""):
      num_units = int(lines[i-1].split("=")[1])
      print_edit(i, 1, f"Found unit player definition (num_units={num_units})")
      for j in range(i+1, len(lines)):
        if lines[j].startswith("}"):
          lines[i-1] = f"nunits={num_units}\n"
          print_edit(j, 1, f"Finished unit player definition (num_units={num_units})")
          break
        any_valid_unit = False
        for unit in valid_units:
          regex = f"\"{unit}\""
          search = re.search(regex, lines[j])
          if search:
            any_valid_unit = True
            break
        if not any_valid_unit:
          # Remove line
          lines_to_erase.append(j)
          num_units -= 1
          print_edit(j, 2, f"Removing {lines[j].strip()}")
  for line in range(len(lines_to_erase)-1, -1, -1):
    lines.pop(lines_to_erase[line])

# Make currently existing units "free" by disassociating their home city
def fix_unit_ownership(lines):
  print("# Fixing unit ownership")
  valid_units = [
    "unit_engineer",
    "unit_banker",
    "unit_scientist",
    "Spy"
  ]
  lines_to_erase = []
  for i in range(len(lines)):
    if lines[i].startswith("u={\"id\""):
      print_edit(i, 1, f"Found unit player definition")
      for j in range(i+1, len(lines)):
        if lines[j].startswith("}"):
          print_edit(j, 1, f"Finished unit player definition")
          break
        unit_def = lines[j].split(",")
        if len(unit_def) > 7:
          unit_def[7] = "0"
          print_edit(j, 2, f"{lines[j]} -> {','.join(unit_def)}")
          lines[j] = ",".join(unit_def) + "\n"
        any_valid_unit = False
        for unit in valid_units:
          regex = f"\"{unit}\""
          search = re.search(regex, lines[j])
          if search:
            any_valid_unit = True
            break
        if not any_valid_unit:
          # Remove line
          lines_to_erase.append(j)
          num_units -= 1
          print_edit(j, 2, f"Removing {lines[j].strip()}")
  for line in range(len(lines_to_erase)-1, -1, -1):
    lines.pop(lines_to_erase[line])


if len(sys.argv) < 3:
  print("Usage: python convert_freeciv_to_lunar_gambit.py <freeciv_in_save_file> <lunar_gambit_out_save_file>")
  sys.exit(1)

freeciv_in_save_file = sys.argv[1]
lunar_gambit_out_save_file = sys.argv[2]

if not os.path.isfile(freeciv_in_save_file):
  print("Error: freeciv_in_save_file does not exist")
  sys.exit(1)

# Read in the freeciv save file
with open(freeciv_in_save_file, "r") as f:
  lines = f.readlines()

  fix_city_production(lines)
  fix_tech(lines)
  fix_player_info(lines)
  fix_server_settings(lines)
  fix_removed_units(lines)
  fix_unit_ownership(lines)

# Write out the lunar gambit save file
with open(lunar_gambit_out_save_file, "w") as f:
  f.writelines(lines)

print(f"File written to {lunar_gambit_out_save_file}. You now need to open this file with the server and then save for it to automatically fix the remaining missing stuff.")
