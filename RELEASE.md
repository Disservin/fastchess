# Releasing Fastchess

Fastchess releases are triggered by pushing a tag whose name starts with `v`.
The release workflow builds binaries, creates a draft GitHub release, generates
release notes, and attaches the archives to the draft.

## Create a release

1. Make sure the commit to release is on `master` and that its required checks
   have passed.
2. Update your local branch:

   ```bash
   git switch master
   git pull --ff-only origin master
   ```

3. Choose a version following the existing `v<major>.<minor>.<patch>` tag
   format, then create and push an annotated tag. For example:

   ```bash
   git tag -a v1.2.3 -m "Fastchess v1.2.3"
   git push origin v1.2.3
   ```

4. Open the [Fastchess Actions page][actions] and follow the workflow run for
   the tag. The release workflow produces archives for Linux, macOS, and
   Windows on the supported architectures.
5. When the workflow succeeds, open the [releases page][releases]. Review the
   generated notes and confirm that all expected archives are attached to the
   draft release.
6. Publish the draft release from GitHub when it is ready.

Do not create the GitHub release manually before pushing the tag. The workflow
creates it as a draft so its files and release notes can be reviewed before it
becomes public.

## Failed workflows

If a transient job fails, rerun the failed jobs from the workflow run on the
Actions page. The release remains a draft until it is manually published. Do
not move or replace a published version tag; use a new version tag for a
corrected release.

[actions]: https://github.com/Disservin/fastchess/actions/workflows/fastchess.yml
[releases]: https://github.com/Disservin/fastchess/releases
