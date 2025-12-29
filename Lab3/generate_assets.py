import os

colors = ['#FF6B6B', '#4ECDC4', '#45B7D1', '#96CEB4', '#FFEEAD', '#D4A5A5']

def create_svg(filename, color, text):
    svg_content = f'''<svg width="300" height="200" xmlns="http://www.w3.org/2000/svg">
    <rect width="100%" height="100%" fill="{color}"/>
    <circle cx="150" cy="100" r="50" fill="rgba(255,255,255,0.3)"/>
    <text x="50%" y="50%" dominant-baseline="middle" text-anchor="middle" font-family="Arial" font-size="24" fill="white">{text}</text>
</svg>'''
    with open(filename, 'w', encoding='utf-8') as f:
        f.write(svg_content)
    print(f"Created {filename}")

def main():
    target_dir = r"c:\学习\计算机网络\Lab3"
    if not os.path.exists(target_dir):
        print(f"Directory not found: {target_dir}")
        return

    os.chdir(target_dir)
    
    for i in range(1, 7):
        create_svg(f"image{i}.svg", colors[i-1], f"Image {i}")

if __name__ == "__main__":
    main()
