# Repository Guidelines

## Project Structure & Module Organization

Work within the repository's existing module layout. Keep implementation code near the component it serves, and place tests beside that component or in its established test directory. Keep generated output, dependencies, and local configuration out of commits. Before adding a new top-level directory, check the current layout with `rg --files` and follow the nearest comparable module.

## Build, Test, and Development Commands

Use the commands defined by the project's checked-in build files and README. From the repository root, inspect available entry points with `rg --files -g 'README*' -g 'package.json' -g 'pyproject.toml' -g 'Makefile' -g 'CMakeLists.txt' -g '*.sln'`. Run the relevant build and test commands before submitting a change. Document any new command in the same file that defines it and include its purpose and prerequisites.

## Coding Style & Naming Conventions

Match the formatting, indentation, file naming, and identifier style of adjacent files. Use existing formatters or linters configured in the repository; avoid unrelated formatting changes. Give new modules and tests names that describe their behavior, and keep public interfaces small and documented where their use is not obvious.

## Testing Guidelines

Add or update tests for changed behavior using the framework already present in the affected module. Name tests after the behavior or regression they cover. Run the focused test suite while developing and the applicable full suite before opening a pull request. If a change cannot be tested automatically, explain the manual checks in the pull request.

## Commit & Pull Request Guidelines

Review recent commit subjects with `git log -10 --pretty=format:%s` and follow their tense, capitalization, and scope pattern. Keep commits focused on one logical change. In pull requests, describe the problem and solution, list the commands run and their results, link any related issue, and add screenshots for visible interface changes. Call out configuration changes and migration steps when applicable.
