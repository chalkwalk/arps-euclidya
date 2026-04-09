import os
import re
import sys

def get_sidebar_order(sidebars_path: str) -> list:
    """Extracts the order of documents from sidebars.ts."""
    if not os.path.exists(sidebars_path):
        print(f"Warning: Sidebar file {sidebars_path} not found.")
        return []
    
    with open(sidebars_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Simple regex to find the items in tutorialSidebar
    match = re.search(r'tutorialSidebar:\s*\[(.*?)\]', content, re.DOTALL)
    if not match:
        print("Warning: tutorialSidebar not found in sidebars.ts.")
        return []
    
    items_block = match.group(1)
    # Extract quoted strings
    items = re.findall(r"['\"](.*?)['\"]", items_block)
    return items

def process_markdown(file_path: str) -> None:
    """Applies transformations to a single markdown file."""
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Pass 1: Strip YAML Frontmatter (Non-greedy match from starting --- to closing ---)
    content = re.sub(r'^---\n.*?\n---\n?', '', content, flags=re.DOTALL)

    # Pass 2: Translate Docusaurus Relative Links to Wiki Flat Links
    # Matches [Link Text](target.md) or [Link Text](./target.md) or [Link Text](../docs/target.md)
    # Transforms to [Link Text](target)
    def link_repl(match):
        text = match.group(1)
        target = match.group(2)
        if target == 'intro':
            return f'[{text}](Home)'
        return f'[{text}]({target})'

    # Pattern matches [text](optional_path/filename.md)
    # We allow safe characters for filename and optional leading path segments
    content = re.sub(r'\[([^\]]+)\]\((?:\.\/|\.\.\/docs\/)?([a-zA-Z0-9_-]+)\.md\)', link_repl, content)

    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)

def generate_sidebar(target_directory: str, order: list, filenames_map: dict) -> None:
    """Generates the _Sidebar.md file for GitHub Wiki."""
    sidebar_path = os.path.join(target_directory, '_Sidebar.md')
    print(f"Generating sidebar at {sidebar_path}")
    with open(sidebar_path, 'w', encoding='utf-8') as f:
        for item in order:
            if item in filenames_map:
                # Determine title: check the file's H1 or fallback to capitalized filename
                title = item.replace('-', ' ').title()
                doc_path = os.path.join(target_directory, filenames_map[item])
                if os.path.exists(doc_path):
                    with open(doc_path, 'r', encoding='utf-8') as doc_f:
                        # Skip empty lines to find the first heading
                        for line in doc_f:
                            clean_line = line.strip()
                            if clean_line.startswith('# '):
                                title = clean_line[2:].strip()
                                break
                            elif clean_line: # Found something that isn't a heading
                                break
                
                wiki_link = 'Home' if item == 'intro' else item
                f.write(f'* [{title}]({wiki_link})\n')
            else:
                print(f"Warning: Sidebar item '{item}' not found in synced files.")

def execute_transformation(target_directory: str, sidebars_path: str) -> None:
    """Main execution loop for transforming the documentation set."""
    # 1. Get ordering
    order = get_sidebar_order(sidebars_path)
    
    # 2. Process all files and rename intro.md to Home.md
    filenames_map = {} # maps sidebar keys (slugs) to current filenames in target_dir
    
    # We'll first collect and rename, then process contents
    for root, _, files in os.walk(target_directory):
        for file in files:
            if file.endswith('.md') and file != '_Sidebar.md':
                old_path = os.path.join(root, file)
                name_no_ext = os.path.splitext(file)[0]
                
                if file == 'intro.md':
                    new_path = os.path.join(root, 'Home.md')
                    os.rename(old_path, new_path)
                    filenames_map['intro'] = 'Home.md'
                    process_markdown(new_path)
                else:
                    filenames_map[name_no_ext] = file
                    process_markdown(old_path)

    # 3. Generate Sidebar
    if order:
        generate_sidebar(target_directory, order, filenames_map)
    else:
        print("No sidebar order found; skipping _Sidebar.md generation.")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python wiki_transform.py <target_directory> <sidebars_path>")
        sys.exit(1)
    
    target_dir = sys.argv[1]
    sidebars_ts = sys.argv[2]
    execute_transformation(target_dir, sidebars_ts)
