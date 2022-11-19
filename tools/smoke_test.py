import base64
import ctypes
import datetime
import imageio.v3
import io
import numpy as np
import os
import re
import signal
import socketserver
import subprocess
import sys
import tempfile
import time
import traceback
import urllib.request

def log_prefix():
    return ">>> Browservice smoke test :: " + datetime.datetime.now().strftime("%Y/%m/%d %H:%M:%S.%f")

def log(msg):
    print(log_prefix(), "--", msg, flush=True)

class FailExit(BaseException):
    pass

def fail(msg):
    print(log_prefix(), "!! FAIL:", msg, flush=True)
    print(log_prefix(), "!! FAIL:", msg, file=sys.stderr, flush=True)
    raise FailExit

def launch_browservice(args, popen_kwargs={}):
    if sys.platform == "linux":
        platform_popen_args = {"preexec_fn": os.setsid}
    else:
        platform_popen_args = {"creationflags": subprocess.CREATE_NEW_CONSOLE}
    process = subprocess.Popen(args, stderr=subprocess.STDOUT, **platform_popen_args, **popen_kwargs)

    if sys.platform == "linux":
        def cleanup():
            os.killpg(os.getpgid(process.pid), signal.SIGKILL)
            process.communicate()
        def ctrlc():
            os.killpg(os.getpgid(process.pid), signal.SIGINT)
    else:
        def cleanup():
            process.kill()
            process.communicate()
        def ctrlc():
            # Sending Ctrl+C on Windows is hacky, adapted from https://stackoverflow.com/a/60795888
            subprocess.check_call([sys.executable, "-c", "import ctypes, sys; kernel=ctypes.windll.kernel32; kernel.FreeConsole(); kernel.AttachConsole(int(sys.argv[1])); kernel.SetConsoleCtrlHandler(None, 1); kernel.GenerateConsoleCtrlEvent(0, 0)", str(process.pid)])

    return (process, {"cleanup": cleanup, "ctrlc": ctrlc})

def version_test(browservice_path):
    log(f"Running version test")

    log(f"Running browservice with --version")
    (process, process_funcs) = launch_browservice([browservice_path, "--version"])

    try:
        process.communicate(timeout=60)
    except:
        process_funcs["cleanup"]()
        raise

    log(f"Browservice exited, checking exit status (should be 1, TODO fix implementation to 0)")
    if process.returncode != 1:
        fail(f"Browservice exit status is {process.returncode} != 1")

    log(f"Version test completed successfully")

def browser_test(browservice_path, allow_online):
    log(f"Running browser test")

    log(f"Finding free port to listen on")
    with socketserver.TCPServer(("127.0.0.1", 0), None) as server:
        port = server.server_address[1]
    log(f"Using port {port}")

    log(f"Running browservice with --vice-opt-http-listen-addr=127.0.0.1:{port}")
    (process, process_funcs) = launch_browservice([browservice_path, f"--vice-opt-http-listen-addr=127.0.0.1:{port}"])

    try:
        prefix = f"http://127.0.0.1:{port}"
        request_timeout = 10
        url = f"{prefix}/"
        log(f"Sending HTTP requests to {url} until it responds")
        attempt_count = 0
        while True:
            attempt_count += 1
            log(f"Sending request #{attempt_count}")
            try:
                with urllib.request.urlopen(url, timeout=request_timeout) as resp:
                    if resp.status == 200:
                        log(f"Got 200 response")
                        resp_html = resp.read().decode("UTF-8")
                        break
            except urllib.error.URLError:
                log(f"Request failed")

            if attempt_count >= 20:
                fail(f"Browservice did not respond to HTTP requests, attempt count limit exhausted")

            log(f"Sleeping 5s and retrying")
            time.sleep(5)

        def process_forward(regex):
            nonlocal resp_html

            log(f"Parsing continuation address from response HTML {repr(resp_html)}")
            match = re.search(regex, resp_html)
            if match is None:
                fail(f"Parsing failed")
            url = f"{prefix}{match.group(1)}"

            log(f"Requesting {url}")
            with urllib.request.urlopen(url, timeout=request_timeout) as resp:
                if resp.status != 200:
                    fail(f"Response status {resp.status} != 200")
                resp_html = resp.read().decode("UTF-8")

        process_forward(r"window\.location\.href = \"(/[0-9+]/[a-zA-Z0-9]+/prev/)\"")
        process_forward(r"window\.location\.href = .\"(/[0-9]+/[a-zA-Z0-9]+/).\"")
        process_forward(r"window\.location\.href = .\"(/[0-9]+/[a-zA-Z0-9]+/next/).\"")
        process_forward(r"window\.location\.href = ...\"(/[0-9]+/[a-zA-Z0-9]+/)...\"")

        log(f"Parsing image request prefix from response HTML of length {len(resp_html)}")
        match = re.search(r"var imgPath =\s+\"(/[0-9]+/[a-zA-Z0-9]+/image/)\"", resp_html)
        if match is None:
            fail(f"Parsing failed")
        img_prefix = f"{prefix}{match.group(1)}"
        log(f"Parsed image prefix {img_prefix}")

        requested_img_width = 654
        requested_img_height = 468

        img_idx = 0
        def fetch_image():
            nonlocal img_idx

            img_idx += 1
            url = f"{img_prefix}1/{img_idx}/1/{requested_img_width}/{requested_img_height}/0/"

            log(f"Requesting image from {url}")
            with urllib.request.urlopen(url, timeout=request_timeout) as resp:
                if resp.status != 200:
                    fail(f"Response status {resp.status} != 200")
                resp_data = resp.read()

            log(f"Loading image data of {len(resp_data)} bytes")
            img = imageio.v3.imread(resp_data)
            if img.dtype != np.uint8:
                fail(f"Image has incorrect datatype {img.dtype} != np.uint8")
            if len(img.shape) != 3:
                fail(f"Image has incorrect dimensionality {len(img.shape)} != 3")
            log(f"Image with shape {img.shape} loaded successfully")
            return img

        def has_correct_shape(img):
            return img.shape[0] >= requested_img_height and img.shape[0] <= requested_img_height + 2 and img.shape[1] >= requested_img_width and img.shape[1] <= requested_img_width + 1 and img.shape[2] == 3

        log(f"Waiting for the image to be of the correct shape")
        attempt_count = 0
        while True:
            attempt_count += 1
            log(f"Requesting image #{attempt_count}")
            img = fetch_image()
            if has_correct_shape(img):
                break

            if attempt_count >= 30:
                fail(f"Image shape did not become correct, attempt count limit exhausted")

            log(f"Sleeping 2s and retrying")
            time.sleep(2)

        log(f"Image of correct shape {img.shape} received")

        def wait_image(criterion):
            attempt_count = 0
            while True:
                attempt_count += 1
                log(f"Requesting image #{attempt_count}")
                img = fetch_image()
                if not has_correct_shape(img):
                    fail(f"Fetched image has incorrect shape {img.shape}")

                if criterion(img):
                    break

                if attempt_count >= 30:
                    fail(f"Image did not fulfill criterion, attempt count limit exhausted")

                log(f"Sleeping 2s and retrying")
                time.sleep(2)

        log(f"Waiting for the image to have non-white control bar and white content")
        def empty_page_brightness_distribution_criterion(img):
            top_mean_brightness = float(np.mean(img[0:27, 0:requested_img_width, :]))
            bottom_mean_brightness = float(np.mean(img[27:requested_img_height, 0:requested_img_width, :]))
            log(f"Image has top mean brightness {top_mean_brightness} and bottom mean brightness {bottom_mean_brightness}")
            return abs(top_mean_brightness - 196.0) < 8.0 and abs(bottom_mean_brightness - 255.0) < 1e-3
        wait_image(empty_page_brightness_distribution_criterion)
        log(f"Image of correct brightness distribution received")

        def goto_url(url):
            with urllib.request.urlopen(f"{img_prefix[:-6]}goto/{url}", timeout=request_timeout) as resp:
                if resp.status != 200:
                    fail(f"Response status {resp.status} != 200")

        log(f"Navigating browser to a data URL that fills the page with text")
        goto_url("data:text/plain;charset=utf-8;base64," + "QUFB" * 1000)

        log(f"Waiting for the content area in the image to become dark enough")
        def criterion(img):
            bottom_mean_brightness = float(np.mean(img[27:requested_img_height, 0:requested_img_width, :]))
            log(f"Image has bottom mean brightness {bottom_mean_brightness}")
            return bottom_mean_brightness >= 128.0 and bottom_mean_brightness <= 224.0
        wait_image(criterion)
        log(f"Image with content area sufficiently dark received")

        test_colors = [[[0, 212, 133], [6, 85, 41], [43, 142, 46], [229, 245, 6]], [[46, 182, 102], [174, 99, 239], [235, 116, 241], [95, 91, 113]]]
        test_img = np.repeat(np.repeat(np.array(test_colors), 150, 0), 150, 1).astype(np.uint8)

        if allow_online:
            url = "https://raw.githubusercontent.com/ttalvitie/browservice/master/tools/smoke_test_data/test_img.png"
            log(f"Navigating browser to the test image URL {url}")
            goto_url(url)
        else:
            log(f"Navigating browser to the test image data URL")
            test_img_data = io.BytesIO()
            imageio.v3.imwrite(test_img_data, test_img, format="png", extension=".png")
            goto_url("data:image/png;base64," + base64.b64encode(test_img_data.getvalue()).decode("ascii"))

        log(f"Waiting for the image to contain the test image colors")
        def test_image_color_criterion(img):
            result = True
            pixel_counts = []
            for test_color_row in test_colors:
                for test_color in test_color_row:
                    pixel_mask = np.all(img == np.reshape(np.array(test_color), (1, 1, 3)).astype(np.uint8), 2)
                    pixel_count = int(np.sum(pixel_mask))
                    pixel_counts.append(pixel_count)
                    if pixel_count < 20000:
                        result = False
            log(f"Pixel counts for each test image color: {pixel_counts}")
            return result
        wait_image(test_image_color_criterion)
        log(f"Image with content area containing the test image colors received")

        log(f"Sending Ctrl+C to Browservice and waiting for it to terminate")
        process_funcs["ctrlc"]()
        process.communicate(timeout=60)
    except:
        process_funcs["cleanup"]()
        raise

    log(f"Browservice exited, checking exit status")
    if process.returncode != 0:
        fail(f"Browservice exit status is {process.returncode} != 0")

    log(f"Browser test completed successfully")

def verdana_installation_test(browservice_path):
    log(f"Running Verdana installation test")

    log(f"Creating fake home directory")
    with tempfile.TemporaryDirectory() as tmpdir:
        log(f"Created fake home directory {tmpdir}")

        log(f"Starting Browservice with --install-verdana with fake home directory")
        env = os.environ.copy()
        env["HOME"] = tmpdir
        (process, process_funcs) = launch_browservice([browservice_path, "--install-verdana"], {"stdin": subprocess.PIPE, "env": env})

        try:
            log(f"Waiting 20s to allow for the process to start")
            time.sleep(20)

            log(f"Sending 'yes' to stdin and waiting for the process to terminate")
            process.communicate(input=b"yes\n", timeout=180)
        except:
            process_funcs["cleanup"]()
            raise

        log(f"Browservice exited, checking exit status")
        if process.returncode != 0:
            fail(f"Browservice exit status is {process.returncode} != 0")

        verdana_path = os.path.join(tmpdir, ".browservice", "appimage", "fonts", "Verdana.ttf")
        log(f"Checking that Verdana was installed to {verdana_path}")
        if not os.path.isfile(verdana_path):
            fail(f"{verdana_path} is not a file")
        verdana_size = os.path.getsize(verdana_path)
        expected_verdana_size = 139640
        if verdana_size != expected_verdana_size:
            fail(f"{verdana_path} has size {verdana_size} != {expected_verdana_size}")

    log(f"Verdana installation test completed successfully")

def main():
    if len(sys.argv) != 3:
        fail(f"Usage: python smoke_test.py <browservice executable path> all|offline")

    browservice_path = sys.argv[1]
    allow_online = {"all": True, "offline": False}[sys.argv[2]]

    version_test(browservice_path)

    browser_test(browservice_path, allow_online)

    if sys.platform == "linux" and allow_online:
        verdana_installation_test(browservice_path)

    log(f"{'All' if allow_online else 'Offline'} Browservice smoke tests completed successfully")

    return 0

if __name__ == "__main__":
    try:
        try:
            main()
        except FailExit:
            raise
        except:
            fail("Unhandled exception:\n" + traceback.format_exc())
    except FailExit:
        sys.exit(1)
