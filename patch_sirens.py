with open("src/features/sirens.cpp", "r") as f:
    text = f.read()

replacement = """	Shadow.Size = Size;

	if (json.contains("size"))
	{"""

if '	Shadow.Size = Size;\n\n	Shadow.Size = Size;\n\n	if (json.contains("size"))\n	{' in text:
    text = text.replace('	Shadow.Size = Size;\n\n	Shadow.Size = Size;\n\n	if (json.contains("size"))\n	{', replacement)

if '			if (std::abs(objPos.x) > std::abs(objPos.y) || std::abs(objPos.x) > 0.1f) {' in text:
    text = text.replace('			if (std::abs(objPos.x) > std::abs(objPos.y) || std::abs(objPos.x) > 0.1f) {', '			if (std::abs(objPos.x) > 0.1f) {')

with open("src/features/sirens.cpp", "w") as f:
    f.write(text)
