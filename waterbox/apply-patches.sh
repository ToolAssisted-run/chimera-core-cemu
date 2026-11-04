#!/bin/sh
# Applies the numbered patches to the extern/cemu submodule (Cemu). Idempotent: a tree
# that already carries the whole series is left alone, a pristine one has it
# applied, and anything in between is an error that names the files.
#
# THE SERIES IS JUDGED AS A WHOLE, not a patch at a time: once two patches
# touch the same place, a fully patched tree neither applies nor reverses the
# earlier one, and a check that asks each patch in turn stops on a tree with
# nothing wrong with it.
#
# So: what the series SHOULD leave behind is worked out on a scratch copy - the
# touched files as the submodule's HEAD has them, with every patch applied in
# order - and the working tree is compared with that. It also catches the state
# the old check could not name: a tree where one file was reverted by hand
# (git checkout -- file), which silently drops that file's share of several
# patches and once produced a core with no rebuild machinery in it at all.
#
# Nintendo's keys, which upstream builds into the program, are taken out
# before the patches, by remove-keys.py rewriting their arrays to zeros. It
# is not a patch, because a patch that removed those lines would carry the
# keys in them; the patches are written against a copy that has been through
# it (make-patches.sh), so their context shows no key either.
#
# The driver lives in waterbox/ and is built OUTSIDE the Cemu tree, so nothing
# is copied in.
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
# CEMU_TREE points it at another checkout: how this script is tested without
# touching the tree a build depends on
cemu="${CEMU_TREE:-$root/extern/cemu}"

[ -n "$(ls "$root"/patches/*.patch 2>/dev/null)" ] || { echo "no patches"; exit 0; }
scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT

# every file the series touches (a created or deleted file has /dev/null on
# one side), and every file the keys are taken out of
{ cat "$root"/patches/*.patch | sed -n 's#^--- a/##p; s#^+++ b/##p'; python3 "$here/remove-keys.py" --list; } | sort -u > "$scratch/touched"

# two scratch copies of those files as HEAD has them (one the series creates
# is absent): head/ as it is, tree/ to become what the series leaves
mkdir "$scratch/head" "$scratch/tree"
while IFS= read -r f; do
	if git -C "$cemu" cat-file -e "HEAD:$f" 2>/dev/null; then
		for d in head tree; do
			mkdir -p "$scratch/$d/$(dirname "$f")"
			git -C "$cemu" show "HEAD:$f" > "$scratch/$d/$f"
			# the mode travels too, or git apply remarks on every executable file
			case "$(git -C "$cemu" ls-tree HEAD -- "$f")" in 100755*) chmod +x "$scratch/$d/$f" ;; esac
		done
	fi
done < "$scratch/touched"

# pristine: the working tree still has HEAD's version of every touched file
pristine=1
while IFS= read -r f; do
	if [ -e "$scratch/head/$f" ]; then
		cmp -s "$scratch/head/$f" "$cemu/$f" || { pristine=0; break; }
	elif [ -e "$cemu/$f" ]; then
		pristine=0; break
	fi
done < "$scratch/touched"

# The keys out and the series applied, on the scratch copy FIRST, whichever
# state the tree is in: a series that does not apply in order must be found
# out here, and not half way through applying it to the real tree.
python3 "$here/remove-keys.py" "$scratch/tree"
for p in "$root"/patches/*.patch; do
	(cd "$scratch/tree" && git apply "$p") || {
		echo "the series does not apply to the submodule's HEAD at $(basename "$p") - was the pin moved without rebasing the patches?" >&2
		exit 1
	}
done

if [ "$pristine" -eq 1 ]; then
	python3 "$here/remove-keys.py" "$cemu"
	echo "Nintendo's keys taken out ($(python3 "$here/remove-keys.py" --list | wc -l) files)"
	for p in "$root"/patches/*.patch; do
		git -C "$cemu" apply "$p"
		echo "applied: $(basename "$p")"
	done
fi

# it must then be EXACTLY what the keys out and the whole series leave behind
wrong=0
while IFS= read -r f; do
	if [ -e "$scratch/tree/$f" ]; then
		cmp -s "$scratch/tree/$f" "$cemu/$f" 2>/dev/null || { echo "not as the series leaves it: $f" >&2; wrong=1; }
	elif [ -e "$cemu/$f" ]; then
		echo "the series deletes this, and it is there: $f" >&2; wrong=1
	fi
done < "$scratch/touched"

if [ "$wrong" -ne 0 ]; then
	echo "extern/cemu is partly patched. To start again from the submodule's HEAD:" >&2
	echo "  git -C extern/cemu reset --hard && git -C extern/cemu clean -fd && waterbox/apply-patches.sh" >&2
	echo "(that discards edits made in the tree - turn them into a patch first)" >&2
	exit 1
fi
python3 "$here/remove-keys.py" "$cemu" --check || { echo "a key is still in the tree" >&2; exit 1; }
[ "$pristine" -eq 1 ] || echo "already applied: all $(ls "$root"/patches/*.patch | wc -l) patches, Nintendo's keys out"
