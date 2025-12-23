import os
import hashlib
import json
import subprocess
import sys
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
    files_to_build = []

    for root, dirs, files in os.walk(FBS_SOURCE_DIR):
        if os.path.abspath(root).startswith(os.path.abspath(CPP_OUTPUT_DIR)):
            continue
            
        for file in files:
            if file.endswith(".fbs"):
                full_path = os.path.normpath(os.path.join(root, file))
                rel_path = os.path.relpath(full_path, FBS_SOURCE_DIR)
                
                current_hash = get_file_hash(full_path)
                new_cache[rel_path] = current_hash

                if rel_path not in old_cache or old_cache[rel_path] != current_hash:
                    files_to_build.append((full_path, rel_path))

    if not files_to_build:
        print("Incremental build: No changes detected.")
        save_cache(new_cache)
        return

    for full_path, rel_path in files_to_build:
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
            "-o", target_out_dir,
            "-I", FBS_SOURCE_DIR,
            full_path
        ]
        
        try:
            subprocess.run(cmd, check=True)
            print(f"Compiled: {rel_path} (Style: {field_style})")
        except subprocess.CalledProcessError:
            print(f"Error compiling: {rel_path}")
            new_cache[rel_path] = old_cache.get(rel_path, "")

    save_cache(new_cache)
    print("Build step finished.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=["build", "clean", "rebuild"], default="build", nargs="?")
    parser.add_argument("--field-style", choices=["original", "lower", "upper"], default="lower", 
                        help="Case style for fields: original, lower (snake_case), or upper (CamelCase)")
    args = parser.parse_args()

    if args.action == "clean":
        clean()
    elif args.action == "rebuild":
        build(is_rebuild=True, field_style=args.field_style)
    else:
        build(field_style=args.field_style)