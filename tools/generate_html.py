"""Generate standalone HTML files from project markdown documentation."""
import markdown
import os
import sys

ROOT = r'D:\ESP32_PROJECT\sound_dsp_project'
OUTPUT_DIR = ROOT


def read_md(rel_path):
    with open(os.path.join(ROOT, rel_path), 'r', encoding='utf-8') as f:
        return f.read()


README_MD = read_md('README.md')
ARCHITECTURE_MD = read_md('docs/architecture.md')
NOTICE_MD = read_md('NOTICE.md')

HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{title}</title>
    <style>
        :root {{
            --bg-color: #f5f6f8;
            --text-main: #24292e;
            --text-light: #586069;
            --sidebar-bg: #ffffff;
            --content-bg: #ffffff;
            --border-color: #e1e4e8;
            --code-bg: #f6f8fa;
            --link-color: #0366d6;
            --blockquote-bg: #f0f4f8;
            --blockquote-border: #c8d1db;
            --table-header-bg: #f6f8fa;
        }}

        html {{
            scroll-behavior: smooth;
        }}

        body {{
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif, "Apple Color Emoji", "Segoe UI Emoji";
            background-color: var(--bg-color);
            color: var(--text-main);
            margin: 0;
            display: flex;
            line-height: 1.6;
            font-size: 16px;
        }}

        .sidebar {{
            width: 280px;
            height: 100vh;
            position: fixed;
            top: 0;
            left: 0;
            background-color: var(--sidebar-bg);
            border-right: 1px solid var(--border-color);
            overflow-y: auto;
            padding: 24px 16px;
            box-sizing: border-box;
            z-index: 100;
        }}

        .sidebar h3 {{
            margin-top: 0;
            padding-bottom: 12px;
            border-bottom: 1px solid var(--border-color);
            font-size: 1.1em;
            color: var(--text-main);
        }}

        .toc ul {{
            list-style: none;
            padding-left: 16px;
            margin: 0;
        }}

        .toc > ul {{
            padding-left: 0;
        }}

        .toc li {{
            margin: 6px 0;
        }}

        .toc a {{
            text-decoration: none;
            color: var(--text-light);
            font-size: 14px;
            display: block;
            padding: 4px 6px;
            border-radius: 4px;
            transition: all 0.2s ease;
        }}

        .toc a:hover {{
            background-color: var(--code-bg);
            color: var(--link-color);
            font-weight: 500;
        }}

        .main-content {{
            margin-left: 280px;
            flex: 1;
            padding: 40px;
            display: flex;
            justify-content: center;
        }}

        .content-wrapper {{
            background: var(--content-bg);
            padding: 50px 60px;
            max-width: 900px;
            width: 100%;
            border-radius: 8px;
            box-shadow: 0 4px 15px rgba(0,0,0,0.04);
            box-sizing: border-box;
        }}

        h1, h2, h3, h4, h5 {{
            color: var(--text-main);
            font-weight: 600;
            margin-top: 1.5em;
            margin-bottom: 0.5em;
        }}

        h1 {{
            font-size: 2.2em;
            border-bottom: 2px solid var(--border-color);
            padding-bottom: 10px;
            margin-top: 0;
        }}

        h2 {{
            font-size: 1.7em;
            border-bottom: 1px solid var(--border-color);
            padding-bottom: 8px;
            margin-top: 2em;
        }}

        h3 {{ font-size: 1.3em; }}
        p {{ margin-bottom: 16px; }}

        pre {{
            background-color: var(--code-bg);
            padding: 16px;
            border-radius: 6px;
            overflow-x: auto;
            font-family: "SFMono-Regular", Consolas, "Liberation Mono", Menlo, monospace;
            font-size: 0.9em;
            border: 1px solid var(--border-color);
        }}

        code {{
            background-color: var(--code-bg);
            padding: 0.2em 0.4em;
            border-radius: 4px;
            font-family: "SFMono-Regular", Consolas, "Liberation Mono", Menlo, monospace;
            font-size: 0.9em;
            color: #cb2431;
        }}

        pre code {{
            color: inherit;
            background: transparent;
            padding: 0;
        }}

        table {{
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
        }}

        th, td {{
            border: 1px solid var(--border-color);
            padding: 10px 16px;
            text-align: left;
        }}

        th {{
            background-color: var(--table-header-bg);
            font-weight: 600;
        }}

        blockquote {{
            border-left: 4px solid var(--blockquote-border);
            margin: 0 0 20px 0;
            padding: 15px 20px;
            background-color: var(--blockquote-bg);
            color: #444d56;
            border-radius: 0 6px 6px 0;
        }}

        blockquote p:last-child {{ margin-bottom: 0; }}
        img {{ max-width: 100%; height: auto; border-radius: 6px; }}

        hr {{
            height: 1px;
            background-color: var(--border-color);
            border: none;
            margin: 40px 0;
        }}
    </style>
</head>
<body>
    <div class="sidebar">
        <h3>📑 目录</h3>
        <div class="toc">
            {toc}
        </div>
    </div>
    <div class="main-content">
        <div class="content-wrapper">
            {content}
        </div>
    </div>
</body>
</html>"""


def generate_html_file(filename, title, md_text):
    md = markdown.Markdown(extensions=['toc', 'fenced_code', 'tables'])
    html_content = md.convert(md_text)
    toc_html = md.toc

    # Escape braces to prevent KeyError from .format()
    html_content = html_content.replace('{', '{{').replace('}', '}}')

    final_html = HTML_TEMPLATE.format(title=title, toc=toc_html, content=html_content)
    outpath = os.path.join(OUTPUT_DIR, filename)
    with open(outpath, 'w', encoding='utf-8') as f:
        f.write(final_html)
    print(f"  ✅ {filename} ({os.path.getsize(outpath):,} bytes)")


if __name__ == '__main__':
    print("Generating HTML documentation...")
    generate_html_file("README.html", "Sound DSP Framework - README", README_MD)
    generate_html_file("ARCHITECTURE.html", "Sound DSP Project - 架构文档", ARCHITECTURE_MD)
    generate_html_file("NOTICE.html", "Sound DSP - 踩坑与演进记录", NOTICE_MD)
    print("🎉 All 3 HTML files generated successfully!")
