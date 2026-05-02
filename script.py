#!/usr/bin/env python3
import os, gzip, sys, argparse

def pad(n): return (4 - (n % 4)) % 4
def read_field(f): return int(f.read(8).decode("ascii"), 16)

def extract_cpio_gz(cpio_gz_path, out_dir="initrd"):
    os.makedirs(out_dir, exist_ok=True)
    with gzip.open(cpio_gz_path, "rb") as f:
        while True:
            magic = f.read(6).decode("ascii")
            if magic != "070701":
                raise ValueError(f"Invalid CPIO magic: {magic}")

            ino      = read_field(f)
            mode     = read_field(f)
            uid      = read_field(f)
            gid      = read_field(f)
            nlink    = read_field(f)
            mtime    = read_field(f)
            filesize = read_field(f)
            devmajor = read_field(f)
            devminor = read_field(f)
            rdevmaj  = read_field(f)
            rdevmin  = read_field(f)
            namesize = read_field(f)
            chksum   = read_field(f)

            name = f.read(namesize).rstrip(b"\x00").decode("utf-8")
            f.read(pad(namesize + 110))

            if name == "TRAILER!!!":
                break

            path = os.path.join(out_dir, name.lstrip("/"))
            if (mode & 0o170000) == 0o040000:  # directory
                os.makedirs(path, exist_ok=True)
                os.chmod(path, mode & 0o7777)
            elif (mode & 0o170000) == 0o120000:  # symlink
                target = f.read(filesize).decode("utf-8")
                f.read(pad(filesize))
                try:
                    os.symlink(target, path)
                except FileExistsError:
                    pass
            else:  # regular file
                os.makedirs(os.path.dirname(path), exist_ok=True)
                data = f.read(filesize)
                with open(path, "wb") as out:
                    out.write(data)
                f.read(pad(filesize))
                os.chmod(path, mode & 0o7777)
    print(f"Extracted {cpio_gz_path} into {out_dir}")

def repack_cpio_gz(src_dir="initrd", out_path="ramdisk.cpio.gz"):
    def write_field(f, val): f.write(f"{val:08x}".encode("ascii"))
    def write_header(f, name, mode, filesize):
        namesize = len(name) + 1
        f.write(b"070701")
        write_field(f, 0)
        write_field(f, mode)
        write_field(f, 0)
        write_field(f, 0)
        write_field(f, 1)
        write_field(f, 0)
        write_field(f, filesize)
        write_field(f, 3)
        write_field(f, 1)
        write_field(f, 0)
        write_field(f, 0)
        write_field(f, namesize)
        write_field(f, 0)
        f.write(name.encode("utf-8") + b"\x00")
        f.write(b"\x00" * pad(namesize + 110))

    with gzip.open(out_path, "wb") as f:
        for root, dirs, files in os.walk(src_dir):
            for dname in dirs:
                path = os.path.join(root, dname)
                rel = "/" + os.path.relpath(path, src_dir)
                st = os.lstat(path)
                mode = st.st_mode & 0o7777 | 0o040000
                write_header(f, rel, mode, 0)
            for fname in files:
                path = os.path.join(root, fname)
                rel = "/" + os.path.relpath(path, src_dir)
                st = os.lstat(path)
                if os.path.islink(path):
                    target = os.readlink(path)
                    mode = st.st_mode & 0o7777 | 0o120000
                    write_header(f, rel, mode, len(target))
                    f.write(target.encode("utf-8"))
                    f.write(b"\x00" * pad(len(target)))
                else:
                    mode = st.st_mode & 0o7777 | 0o100000
                    filesize = os.path.getsize(path)
                    write_header(f, rel, mode, filesize)
                    with open(path, "rb") as inp:
                        f.write(inp.read())
                    f.write(b"\x00" * pad(filesize))

        write_header(f, "TRAILER!!!", 0, 0)
        f.write(b"\x00" * pad(110 + len("TRAILER!!!") + 1))
    print(f"Packed {src_dir} into {out_path}")

def main():
    parser = argparse.ArgumentParser(description="CPIO ramdisk extractor/repacker with permissions preserved")
    parser.add_argument("-i", "--input", help="Input file (for unpack) or input directory (for repack)")
    parser.add_argument("-o", "--output", help="Output directory (for unpack) or output file (for repack)")
    parser.add_argument("-u", "--unpack", action="store_true", help="Unpack input into directory (default initrd)")
    parser.add_argument("-r", "--repack", action="store_true", help="Repack directory into gzipped cpio (default ramdisk.cpio.gz)")
    parser.add_argument("positional", nargs="?", help="Optional positional shortcut")

    args = parser.parse_args()

    if args.unpack:
        infile = args.input or args.positional or "ramdisk.cpio.gz"
        outdir = args.output or "initrd"
        extract_cpio_gz(infile, outdir)
    elif args.repack:
        indir = args.input or "initrd"
        outfile = args.output or args.positional or "ramdisk.cpio.gz"
        repack_cpio_gz(indir, outfile)
    else:
        parser.error("Must specify either --unpack (-u) or --repack (-r)")

if __name__ == "__main__":
    main()
