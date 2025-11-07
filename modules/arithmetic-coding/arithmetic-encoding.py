from scipy.stats import entropy
from tqdm import tqdm
import time
import pandas as pd
from decimal import Decimal, getcontext
import os
import json
import argparse

def encode_arithmetic(sequence: list, distribution: list, verbose: bool = True):
    """
    Given a list of symbols (0 <= sequence[i] < vocab_size), and probability distribution p[i],
    returns a single binary decimal (float in [0,1)).
    """
    start_time = time.time()
    # Number of symbols to encode
    N = len(sequence)
    # Increase precision for long sequences
    precision = max(50, int(entropy(distribution, base=10) * N * 1.2))
    getcontext().prec = precision

    if abs(sum(distribution) - 1.0) > 1e-8:
        raise ValueError("Distribution must sum to 1.")

    # Convert to Decimal
    distribution = [Decimal(str(p)) for p in distribution]

    # compute cdf
    cdf = [Decimal('0')]
    for p in distribution:
        cdf.append(cdf[-1] + p)

    low = Decimal('0')
    high = Decimal('1')

    if verbose:
        print("Encoding...")
        iterator = tqdm(sequence)
    else:
        iterator = sequence

    for s in iterator:
        symbol_low = cdf[s]
        symbol_high = cdf[s+1]
        range_ = high - low
        high = low + range_ * symbol_high
        low = low + range_ * symbol_low

    elapsed = time.time() - start_time
    if verbose:
        print(f"Encoding completed in {elapsed:.4f} seconds.")

    # Return the midpoint of [low, high) for better decoding robustness
    return (low + high) / 2, elapsed

def decode_arithmetic(value: Decimal, N: int, distribution: list, verbose: bool = True):
    """
    Given a decimal in [0,1), sequence length N, and probability distribution p[i],
    returns encoded list of symbols (0 <= symbols[i] < vocab_size) 
    """
    start_time = time.time()
    precision = int(entropy(distribution, base=10)*N*1.2)
    getcontext().prec = precision

    if abs(sum(distribution) - 1.0) > 1e-8:
        raise ValueError("Distribution must sum to 1.")

    # convert to Decimal
    distribution = [Decimal(str(p)) for p in distribution]
    
    # compute cdf 
    cdf = [Decimal('0')]
    for p in distribution:
        cdf.append(cdf[-1] + p)

    sequence = []
    low = Decimal('0')
    high = Decimal('1')
    
    if verbose:
        print("Decoding...")
        iterator = tqdm(range(N))
    else:
        iterator = range(N)

    for _ in iterator:
        range_ = high - low
        target = (value - low) / range_

        # Manual binary search for Decimal => SLOW! We shouldn't be relying on native comparisons in Decimal
        left, right = 0, len(cdf) - 2
        while left <= right:
            mid = (left + right) // 2
            if cdf[mid] <= target < cdf[mid + 1]:
                idx = mid
                break
            elif target < cdf[mid]:
                right = mid - 1
            else:
                left = mid + 1

        sequence.append(idx)
        high = low + range_ * cdf[idx + 1]
        low = low + range_ * cdf[idx]

    elapsed = time.time() - start_time
    if verbose:
        print(f"Decoding completed in {elapsed:.4f} seconds.")

    return sequence, elapsed 

def encode_string(s: str, verbose: bool = True):
    """
    Encodes a string using arithmetic encoding based on character frequencies.
    """
    # Compute character frequencies
    counts = {}
    for char in s:
        counts[char] = counts.get(char, 0) + 1
    total = len(s)
    chars = sorted(counts.keys())
    distribution = [counts[char]/total for char in chars]

    if verbose:
        print("\nCharacter distribution:")
        print(pd.DataFrame(index=[f"'{c}'" for c in chars], columns=["frequency"], data=distribution).sort_values(by="frequency", ascending=False).head(10))
        print()

    # Map characters to indices
    char_to_idx = {char: idx for idx, char in enumerate(chars)}
    sequence = [char_to_idx[char] for char in s]

    encoded_value, elapsed = encode_arithmetic(sequence, distribution, verbose=verbose)

    return encoded_value, elapsed, chars, distribution

def decode_string(value: Decimal, N: int, chars: list, distribution: list, verbose: bool = True):
    """
    Decodes a string from an encoded decimal value using arithmetic decoding.
    """
    sequence, elapsed = decode_arithmetic(value, N, distribution, verbose=verbose)
    idx_to_char = {idx: char for idx, char in enumerate(chars)}
    decoded_string = ''.join(idx_to_char[idx] for idx in sequence)
    return decoded_string, elapsed


def encode_bytes(data: bytes, verbose: bool = True):
    """
    Encodes raw bytes using arithmetic encoding. Treats each distinct byte value present
    as a symbol in the alphabet to reduce alphabet size when possible.
    Returns: encoded Decimal value, elapsed time, symbols (list of int), distribution (list of float)
    """
    # Compute byte frequencies
    counts = {}
    for b in data:
        counts[b] = counts.get(b, 0) + 1
    total = len(data)
    symbols = sorted(counts.keys())
    distribution = [counts[s] / total for s in symbols]

    if verbose:
        print(f"\nByte alphabet size: {len(symbols)}; total bytes: {total}")

    # Map symbol (byte) to index
    sym_to_idx = {s: i for i, s in enumerate(symbols)}
    sequence = [sym_to_idx[b] for b in data]

    encoded_value, elapsed = encode_arithmetic(sequence, distribution, verbose=verbose)

    return encoded_value, elapsed, symbols, distribution


def compress_file(path: str, out_dir: str = None, verbose: bool = True):
    """
    Compress a single file (binary) using arithmetic encoding and write a JSON file
    containing the encoded decimal and metadata to the output directory (defaults to file's folder or provided out_dir).
    """
    if not os.path.isfile(path):
        raise FileNotFoundError(path)

    with open(path, 'rb') as f:
        data = f.read()

    encoded_value, enc_time, symbols, distribution = encode_bytes(data, verbose=verbose)

    # Prepare output
    base = os.path.splitext(os.path.basename(path))[0]
    out_folder = out_dir or os.path.dirname(path)
    os.makedirs(out_folder, exist_ok=True)
    out_path = os.path.join(out_folder, f"{base}.arith.json")

    payload = {
        'encoded_decimal': str(encoded_value),
        'length': len(data),
        'symbols': symbols,
        'distribution': distribution
    }

    with open(out_path, 'w', encoding='utf-8') as out:
        json.dump(payload, out)

    if verbose:
        print(f"Wrote compressed file to: {out_path} (encoding time {enc_time:.4f}s)")

    return out_path


def compress_all_in_dir(dir_path: str, out_dir: str = None, verbose: bool = True):
    """
    Compress all regular files in `dir_path` (non-recursive) and write outputs into out_dir (or dir_path).
    Skips files that already end with `.arith.json`.
    Returns list of output paths.
    """
    out_paths = []
    for name in os.listdir(dir_path):
        src = os.path.join(dir_path, name)
        if not os.path.isfile(src):
            continue
        if name.endswith('.arith.json'):
            continue
        try:
            out = compress_file(src, out_dir=out_dir, verbose=verbose)
            out_paths.append(out)
        except Exception as e:
            print(f"Failed to compress {src}: {e}")
    return out_paths


def decompress_file(arith_json_path: str, out_path: str = None, verbose: bool = True):
    """
    Read an `.arith.json` file produced by `compress_file` and decode it back to bytes,
    writing a file to out_path if provided (defaults to same folder with `.decoded` suffix).
    This uses the same arithmetic decoder and may be slow for large files.
    """
    with open(arith_json_path, 'r', encoding='utf-8') as f:
        payload = json.load(f)

    encoded_decimal = Decimal(payload['encoded_decimal'])
    length = int(payload['length'])
    symbols = payload['symbols']
    distribution = payload['distribution']

    sequence, dec_time = decode_arithmetic(encoded_decimal, length, distribution, verbose=verbose)

    # Map indices back to bytes
    idx_to_sym = {i: s for i, s in enumerate(symbols)}
    data = bytes(idx_to_sym[idx] for idx in sequence)

    if out_path is None:
        base = os.path.splitext(os.path.basename(arith_json_path))[0]
        out_path = os.path.join(os.path.dirname(arith_json_path), f"{base}.decoded")

    with open(out_path, 'wb') as out:
        out.write(data)

    if verbose:
        print(f"Wrote decompressed file to: {out_path} (decoding time {dec_time:.4f}s)")

    return out_path


def _sample_text_roundtrip():
    with open(os.path.join(os.path.dirname(__file__), "inputs", "500.txt"), "r", encoding='utf-8') as f:
        s = f.read()
    N = len(s)
    print("\nInput size:", N, "characters")

    encoded, enc_time, chars, distribution = encode_string(s, verbose=True)
    decoded, dec_time = decode_string(encoded, N, chars, distribution, verbose=True)

    # Verify correctness
    import sys
    if s == decoded:
        print(f"\nSuccessfully encoded and decoded {N} symbols.")
        compression_ratio = sys.getsizeof(encoded) / sys.getsizeof(s)
        print(f"Compressed size: {100*compression_ratio:.5f}%")
    else:
        for i in range(N):
            if s[i] != decoded[i]:
                print(f"\nMismatch at position {i}: original {s[i]}, decoded {decoded[i]}")
                break


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Arithmetic encoder for strings and binary files')
    parser.add_argument('--file', '-f', help='Path to a single file to compress (binary).')
    parser.add_argument('--all', action='store_true', help='Compress all files in the repository images directory (non-recursive).')
    parser.add_argument('--outdir', '-o', help='Directory to write compressed outputs. Defaults to project images folder.')
    parser.add_argument('--decompress', action='store_true', help='If set with a .arith.json file, decompress instead of compressing.')
    parser.add_argument('--no-verbose', action='store_true', help='Suppress verbose tqdm/prints')
    args = parser.parse_args()

    verbose = not args.no_verbose

    repo_root = os.path.dirname(os.path.dirname(__file__))
    default_images = os.path.join(repo_root, 'images')
    outdir = args.outdir or default_images

    if args.file:
        src = args.file
        if args.decompress:
            decompress_file(src, out_path=None, verbose=verbose)
        else:
            compress_file(src, out_dir=outdir, verbose=verbose)
    elif args.all:
        compress_all_in_dir(default_images, out_dir=outdir, verbose=verbose)
    else:
        # no args: run small text sample roundtrip
        _sample_text_roundtrip()