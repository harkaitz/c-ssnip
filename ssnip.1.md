# SSNIP

Automatic code section replacement tool.

# Synopsis

    ssnip [-lpsiq] [FILES...]

# Description

This tool helps updating common code sections across projects. For this it
reads templates from a directory.

# Templates

All templates are searched for in *\${PREFIX}/share/ssnip* by default. You can change
this directory by changing the *\${SSNIP_TEMPLATE_DIR}* environment directory.

There are two types of templates:

- Script templates : Start with a `#!`.
- Text templates   : Have `#s:` sections, these are stripped.

How the sections in the code are detected and matched with templates is done by tags
defined at the beggining of the template file.

- *#s:open_regex:REGEX* : Matches a section's start.
- *#s:quit_regex:REGEX* : Matches a section's end (not inclusive).
- *#s:inside_regex:REGEX* : Whilst the section started, close on a line that doesn't match REGEX.
- *#s:filename_regex:REGEX* : Match the whole file.

If a script template returns 2 then the replacement is aborted. The script can read the old section
by accessing *\${SSNIP_OLD}* environment variable. It can also get the filename from *\${SSNIP_FILE}*.

# Command line arguments

The executable's name is *ssnip*.

- *-l* : List templates.
- *-p TEMPLATE* : Print template's result.
- *-s FILE* : Print how the file would be processed.
- *-i* : Interactive, print a diff and ask before replacing.
- *-q* : Quiet mode.

# Authors

Harkaitz Agirre <harkaitz.aguirre@gmail.com>.

# Collaboration

For making bug reports, feature requests and donations visit
one of the following links:

1. [gemini://harkadev.com/oss/](gemini://harkadev.com/oss/)
2. [https://harkadev.com/oss/](https://harkadev.com/oss/)
# See also

**REGEX(7)**, **SH(1)**
