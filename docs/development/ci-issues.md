# CI issue report

The detailed workflow failure analysis and fixes are kept in the repository
root as [`CI_ISSUES_AND_FIXES.md`](https://github.com/xsession/openFPGALoader/blob/master/CI_ISSUES_AND_FIXES.md).

The live workflow is available at
[GitHub Actions](https://github.com/xsession/openFPGALoader/actions).

The documentation job now builds this MkDocs site with `mkdocs build --strict`.
The workflow also runs the compatibility-data generator before building so the
board, FPGA, and cable tables stay synchronized with their YAML sources.
