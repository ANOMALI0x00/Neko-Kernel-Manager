import sys
import re

def reformat_qml(content):
    # Split into lines
    lines = content.split('\n')
    new_lines = []
    
    for line in lines:
        # Pre-process: split lines that have multiple braces or content before/after braces
        # But try to avoid breaking one-liners if they are simple (e.g. MouseArea { anchors.fill: parent })
        
        # If line has multiple } or {
        # Or if it has } followed by something else
        # Or something else followed by {
        
        processed_line = line.strip()
        if not processed_line:
            new_lines.append("")
            continue
            
        # Very simple one-liners to keep: property, signal, and simple objects
        # e.g. Rectangle { width: 10; height: 10 }
        # If it fits on one line and doesn't contain nested objects, we might keep it.
        # But the user specifically asked for every { and } on its own line for better debugging.
        
        # Strategy: 
        # 1. Replace all '{' with '\n{\n'
        # 2. Replace all '}' with '\n}\n'
        # 3. Clean up (remove empty lines, strip whitespace)
        # 4. Re-indent
        
        # Exception: don't split if it's inside a string or comment
        # This is hard with regex, let's do a simple character-by-character pass
        
        pass_1 = []
        in_string = False
        in_comment = False
        string_char = ""
        
        temp = ""
        i = 0
        while i < len(line):
            c = line[i]
            if in_comment:
                temp += c
                i += 1
                continue
            
            if not in_string:
                if c == '"' or c == "'":
                    in_string = True
                    string_char = c
                    temp += c
                elif c == '/' and i + 1 < len(line) and line[i+1] == '/':
                    in_comment = True
                    temp += c
                elif c == '{':
                    if temp.strip():
                        pass_1.append(temp.strip())
                    pass_1.append("{")
                    temp = ""
                elif c == '}':
                    if temp.strip():
                        pass_1.append(temp.strip())
                    pass_1.append("}")
                    temp = ""
                else:
                    temp += c
            else:
                temp += c
                if c == string_char and line[i-1] != '\\':
                    in_string = False
            i += 1
        if temp.strip():
            pass_1.append(temp.strip())
            
        new_lines.extend(pass_1)

    # Now re-indent
    reformatted = []
    indent_level = 0
    indent_size = 4
    
    for line in new_lines:
        line = line.strip()
        if not line:
            reformatted.append("")
            continue
            
        if line == "}":
            indent_level -= 1
            
        reformatted.append(" " * (indent_level * indent_size) + line)
        
        if line == "{":
            indent_level += 1
            
    return "\n".join(reformatted), indent_level

with open('src/Main.qml', 'r') as f:
    content = f.read()

reformatted_content, final_indent = reformat_qml(content)

# Special case for some one-liners that might have been broken too much or need joining
# Actually, the user wants them on separate lines mostly.

# Write it out
with open('src/Main.qml.new', 'w') as f:
    f.write(reformatted_content)

print(f"Final indent level: {final_indent}")
if final_indent == 0:
    print("Braces are balanced.")
else:
    print("Braces are NOT balanced!")
