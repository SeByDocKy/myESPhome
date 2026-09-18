#!/usr/bin/env python3
import sys
import requests
import json

BASE_URL = "https://app.api.apsystemsema.com:9223"
LANGUAGE = "de_DE"

APP_ID = "4029817264d4821d0164d4821dd80015"
APP_SECRET = "EZAd2023"


def pretty_print(title, data):
    print(f"\n=== {title} ===")
    print(json.dumps(data, indent=2, ensure_ascii=False))


def post(path, *, headers=None, params=None, data=None, title=None):
    url = f"{BASE_URL}{path}"
    merged_headers = {
        "Accept-Language": LANGUAGE,
        "User-Agent": "okhttp/4.9.0",
        "Content-Type": "application/x-www-form-urlencoded",
        **(headers or {}),
    }

    response = requests.post(url, headers=merged_headers, params=params, data=data)
    response.raise_for_status()
    result = response.json()

    if title:
        pretty_print(title, result)
    return result


def get_bearer_token():
    result = post(
        "/api/token/generateToken/application",
        params={"language": LANGUAGE},
        data={"language": LANGUAGE, "app_id": APP_ID, "app_secret": APP_SECRET},
        title="Generate Token Response",
    )
    data = result["data"]
    return data["access_token"], data["refresh_token"]


def refresh_token(refresh_token):
    result = post(
        "/api/token/refreshToken",
        params={"language": LANGUAGE},
        data={"refresh_token": refresh_token},
        title="Refresh Token Response",
    )
    return result["data"]["access_token"]


def get_latest_version(access_token, device_id):
    headers = {"Authorization": f"Bearer {access_token}"}

    data = {
        "language": LANGUAGE,
        "deviceDevId": device_id,
        "type": "EZ1",
        "version": "EZ1 1.7.0",
        "moduleVersion": '{"DSP":"5251","DCM":"1.2.35"}',
    }

    return post(
        f"/aps-api-web/api/v2/remote/common/latestEdition/EZ1/{device_id}",
        headers=headers,
        params={"language": LANGUAGE},
        data=data,
        title="Latest Edition Response",
    )


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(f"Usage: {sys.argv[0]} <DEVICE_ID>")

    DEVICE_ID = sys.argv[1]

    access_token, refresh_token = get_bearer_token()
    # access_token = refresh_token(refresh_token)  # optional
    get_latest_version(access_token, DEVICE_ID)
