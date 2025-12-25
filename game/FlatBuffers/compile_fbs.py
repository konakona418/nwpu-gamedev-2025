import os
import hashlib
import json
import subprocess
import re
import shutil
import argparse

FBS_SOURCE_DIR = "./"
CPP_OUTPUT_DIR = "./Generated"
CACHE_FILE = ".fbs_cache.json"
FLATC_PATH = "flatc"

def get_file_hash(filepath):
    hasher = hashlib.sha256()
    with open(filepath, 'rb') as f:
        buf = f.read()
        hasher.update(buf)
    return hasher.hexdigest()

def get_includes(filepath):
    includes = []
    pattern = re.compile(r'include\s+"([^"]+)"\s*;')
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            for line in f:
                match = pattern.search(line)
                if match:
                    includes.append(match.group(1))
    except Exception:
        pass
    return includes

def load_cache():
    if os.path.exists(CACHE_FILE):
        with open(CACHE_FILE, 'r') as f:
            return json.load(f)
    return {}

def save_cache(cache):
    with open(CACHE_FILE, 'w') as f:
        json.dump(cache, f, indent=4)

def clean():
    if os.path.exists(CPP_OUTPUT_DIR):
        shutil.rmtree(CPP_OUTPUT_DIR)
    if os.path.exists(CACHE_FILE):
        os.remove(CACHE_FILE)
    print(f"Removed {CPP_OUTPUT_DIR} and {CACHE_FILE}")

def build(is_rebuild=False, field_style="original"):
    if is_rebuild:
        clean()

    if not os.path.exists(CPP_OUTPUT_DIR):
        os.makedirs(CPP_OUTPUT_DIR)

    old_cache = load_cache()
    new_cache = {}
    all_fbs_files = {}
    
    for root, dirs, files in os.walk(FBS_SOURCE_DIR):
        if os.path.abspath(root).startswith(os.path.abspath(CPP_OUTPUT_DIR)):
            continue
        for file in files:
            if file.endswith(".fbs"):
                full_path = os.path.normpath(os.path.join(root, file))
                rel_path = os.path.relpath(full_path, FBS_SOURCE_DIR)
                all_fbs_files[rel_path] = full_path
                new_cache[rel_path] = get_file_hash(full_path)

    affected_by = {rel: [] for rel in all_fbs_files}
    for rel_path, full_path in all_fbs_files.items():
        deps = get_includes(full_path)
        for dep in deps:
            dep_rel = os.path.normpath(os.path.join(os.path.dirname(rel_path), dep))
            if dep_rel in affected_by:
                affected_by[dep_rel].append(rel_path)

    dirty_files = set()
    for rel_path in all_fbs_files:
        if rel_path not in old_cache or old_cache[rel_path] != new_cache[rel_path]:
            dirty_files.add(rel_path)

    files_to_build_rel = set()
    def mark_dirty(rel):
        if rel in files_to_build_rel:
            return
        files_to_build_rel.add(rel)
        for parent in affected_by.get(rel, []):
            mark_dirty(parent)

    for dirty in dirty_files:
        mark_dirty(dirty)

    if not files_to_build_rel:
        print("Incremental build: No changes detected.")
        save_cache(new_cache)
        return

    print(f"Building {len(files_to_build_rel)} files (including dependencies)...")
    for rel_path in files_to_build_rel:
        full_path = all_fbs_files[rel_path]
        rel_dir = os.path.dirname(rel_path)
        target_out_dir = os.path.normpath(os.path.join(CPP_OUTPUT_DIR, rel_dir))
        
        if not os.path.exists(target_out_dir):
            os.makedirs(target_out_dir)

        cmd = [
            FLATC_PATH, 
            "--cpp", 
            "--cpp-std", "c++17",
            "--cpp-static-reflection", 
            "--cpp-str-flex-ctor",
            "--scoped-enums", 
            "--cpp-field-case-style", field_style,
            "--gen-object-api",
            "-o", target_out_dir, "-I", FBS_SOURCE_DIR, full_path
        ]
        
        try:
            subprocess.run(cmd, check=True)
            print(f"Compiled: {rel_path}")
        except subprocess.CalledProcessError:
            print(f"Error compiling: {rel_path}")
            new_cache[rel_path] = old_cache.get(rel_path, "")

    save_cache(new_cache)
    print("Build step finished.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=["build", "clean", "rebuild"], default="build", nargs="?")
    parser.add_argument("--field-style", choices=["original", "lower", "upper"], default="lower")
    args = parser.parse_args()

    if args.action == "clean":
        clean()
    elif args.action == "rebuild":
        build(is_rebuild=True, field_style=args.field_style)
    else:
        build(field_style=args.field_style)