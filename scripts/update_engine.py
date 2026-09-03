#!/usr/bin/env python3
"""
TTBOX Update Engine - 设备端更新引擎
独立运行，与 TTBOX Core 解耦。

运行方式:
  python3 update_engine.py [--action ACTION] [--version VERSION] [--server URL]
"""

import os, sys, json, hashlib, base64, time, shutil, tarfile, socket, logging, argparse
from pathlib import Path

# ── 常量 ──
VERSION = '0.1.0'
ETC_DIR = '/etc/ttbox'
VAR_DIR = '/var/lib/ttbox'
UPDATE_DIR = '/var/lib/ttbox/update'
STAGING_DIR = '/var/lib/ttbox/update/staging'
BACKUP_DIR = '/var/lib/ttbox/update/backup'
DOWNLOADS_DIR = '/var/lib/ttbox/update/downloads'
KEYS_DIR = '/var/lib/ttbox/update/trusted_keys'
RUN_DIR = '/run/ttbox'
STATE_FILE = '/var/lib/ttbox/update/update_state.json'
LOCK_FILE = '/run/ttbox/update.lock'
LOG_DIR = '/var/log/ttbox'
LOG_FILE = '/var/log/ttbox/update.log'
DEFAULT_SERVER = 'http://127.0.0.1:8081'

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    handlers=[logging.StreamHandler()]
)
log = logging.getLogger('update-engine')


class UpdateEngine:
    def __init__(self, server_url=DEFAULT_SERVER):
        self.server_url = server_url
        self.state = self._load_state()
        self._ensure_dirs()

    def _ensure_dirs(self):
        for d in [ETC_DIR, VAR_DIR, UPDATE_DIR, STAGING_DIR, BACKUP_DIR,
                  DOWNLOADS_DIR, KEYS_DIR, RUN_DIR, LOG_DIR]:
            os.makedirs(d, exist_ok=True)

    def _load_state(self):
        if os.path.exists(STATE_FILE):
            try:
                with open(STATE_FILE) as f:
                    return json.load(f)
            except:
                pass
        return {
            'state': 'IDLE', 'current_version': '0.0.0', 'previous_version': '',
            'last_update_time': '', 'attempted_version': '', 'attempted_channel': '',
            'error_count': 0, 'last_error': '', 'rollback_available': False, 'backup_path': ''
        }

    def _save_state(self):
        with open(STATE_FILE, 'w') as f:
            json.dump(self.state, f, indent=2)

    def _set_state(self, new_state):
        log.info(f'State: {self.state["state"]} -> {new_state}')
        self.state['state'] = new_state
        self._save_state()

    def _load_public_key(self):
        for f_name in os.listdir(KEYS_DIR):
            if f_name.endswith('.pub'):
                with open(os.path.join(KEYS_DIR, f_name)) as kf:
                    return kf.read()
        key_path = os.path.join(ETC_DIR, 'release-public.pem')
        if os.path.exists(key_path):
            with open(key_path) as f:
                return f.read()
        log.error('No trusted public key found')
        return None

    def _verify_signature(self, sha256_hash, signature_b64):
        from cryptography.hazmat.primitives.asymmetric import ed25519
        from cryptography.hazmat.primitives import serialization
        pem = self._load_public_key()
        if not pem:
            return False
        try:
            public_key = serialization.load_pem_public_key(pem.encode())
            signature = base64.b64decode(signature_b64)
            public_key.verify(signature, sha256_hash.encode())
            return True
        except Exception as e:
            log.error(f'Signature verification failed: {e}')
            return False

    def _http_get(self, url):
        import urllib.request
        req = urllib.request.Request(url, method='GET')
        with urllib.request.urlopen(req, timeout=30) as resp:
            return resp.read(), resp.status

    def get_status(self):
        return {
            'ok': True,
            'data': {
                'state': self.state['state'],
                'current_version': self.state['current_version'],
                'previous_version': self.state['previous_version'],
                'last_update_time': self.state['last_update_time'],
                'attempted_version': self.state['attempted_version'],
                'error_count': self.state['error_count'],
                'last_error': self.state['last_error'],
                'rollback_available': self.state['rollback_available']
            }
        }

    def check_update(self):
        if self.state['state'] not in ['IDLE', 'FAILED', 'ROLLED_BACK']:
            return {'ok': False, 'error': 'Update in progress'}

        self._set_state('CHECKING')
        try:
            import urllib.request
            url = f'{self.server_url}/api/update/check'
            body = json.dumps({
                            'product': 'TTBOX',
                            'current_version': self.state['current_version'],
                            'hardware': 'rk3588',
                            'channel': getattr(self, 'channel', 'stable'),
                            'components': ['core', 'web', 'gateway']
                        }).encode()
            req = urllib.request.Request(url, data=body, method='POST')
            req.add_header('Content-Type', 'application/json')
            with urllib.request.urlopen(req, timeout=30) as resp:
                result = json.loads(resp.read().decode())

            if not result.get('ok'):
                self._set_state('IDLE')
                return result

            data = result.get('data', {})
            if data.get('update_available'):
                self.state['attempted_version'] = data['latest_version']
                self.state['attempted_channel'] = data.get('channel', 'stable')
                self._save_state()
                self._set_state('IDLE')
                return {
                    'ok': True,
                    'data': {
                        'update_available': True,
                        'latest_version': data['latest_version'],
                        'release_date': data.get('release_date', ''),
                        'release_notes_url': data.get('release_notes_url', ''),
                        'manifest_url': data.get('manifest_url', '')
                    }
                }
            else:
                self._set_state('IDLE')
                return {'ok': True, 'data': {'update_available': False, 'message': 'Already up to date'}}

        except Exception as e:
            log.error(f'Check update failed: {e}')
            self.state['last_error'] = str(e)
            self.state['error_count'] += 1
            self._save_state()
            self._set_state('IDLE')
            return {'ok': False, 'error': str(e)}

    def download_update(self, version=None):
        version = version or self.state['attempted_version']
        if not version:
            return {'ok': False, 'error': 'No version specified'}

        self._set_state('DOWNLOADING')

        try:
            channel = self.state.get('attempted_channel', 'stable')
            manifest_url = f'{self.server_url}/api/update/manifest/TTBOX/{version}/{channel}/manifest.json'
            manifest_data, status = self._http_get(manifest_url)
            manifest = json.loads(manifest_data.decode())

            total_size = sum(p.get('size', 0) for p in manifest.get('packages', []))

            downloaded = []
            for pkg in manifest.get('packages', []):
                pkg_url = f'{self.server_url}{pkg["url"]}'
                pkg_path = os.path.join(DOWNLOADS_DIR, pkg['package_id'])

                log.info(f'Downloading: {pkg["package_id"]}')
                data, status = self._http_get(pkg_url)

                with open(pkg_path, 'wb') as f:
                    f.write(data)

                # 验证 SHA256
                actual_hash = hashlib.sha256(data).hexdigest()
                if actual_hash != pkg['sha256']:
                    os.remove(pkg_path)
                    self._set_state('FAILED')
                    self.state['last_error'] = f'SHA256 mismatch for {pkg["package_id"]}'
                    self._save_state()
                    return {'ok': False, 'error': 'SHA256 mismatch'}

                # 验证 Ed25519 签名（使用 manifest 中的 signature 字段，它是对 SHA256 的签名）
                manifest_sig = pkg.get('signature') or manifest.get('signature', '')
                if manifest_sig:
                    if not self._verify_signature(pkg['sha256'], manifest_sig):
                        os.remove(pkg_path)
                        self._set_state('FAILED')
                        self.state['last_error'] = f'Signature verification failed for {pkg["package_id"]}'
                        self._save_state()
                        return {'ok': False, 'error': 'Signature verification failed'}
                    log.info(f'Signature verified for {pkg["package_id"]}')

                downloaded.append(pkg_path)

            self.state['attempted_version'] = version
            self._save_state()
            self._set_state('VERIFYING')

            return {
                'ok': True,
                'data': {'version': version, 'packages': len(downloaded), 'total_size': total_size}
            }

        except Exception as e:
            log.error(f'Download failed: {e}')
            self.state['last_error'] = str(e)
            self.state['error_count'] += 1
            self._save_state()
            self._set_state('FAILED')
            return {'ok': False, 'error': str(e)}

    def stage_update(self):
        if not self.state.get('attempted_version'):
            return {'ok': False, 'error': 'No update version selected'}
        self._set_state('STAGING')
        try:
            version = self.state['attempted_version']
            staging_dir = os.path.join(STAGING_DIR, f'v{version}')
            if os.path.exists(staging_dir):
                shutil.rmtree(staging_dir)

            for f_name in os.listdir(DOWNLOADS_DIR):
                if f_name.endswith('.tar.gz'):
                    pkg_path = os.path.join(DOWNLOADS_DIR, f_name)
                    with tarfile.open(pkg_path, 'r:gz') as tar:
                        tar.extractall(path=staging_dir)

            log.info(f'Staged to: {staging_dir}')
            self._set_state('READY')
            return {'ok': True, 'data': {'staging_dir': staging_dir}}
        except Exception as e:
            log.error(f'Stage failed: {e}')
            self.state['last_error'] = str(e)
            self._save_state()
            self._set_state('FAILED')
            return {'ok': False, 'error': str(e)}

    def apply_update(self):
        if not self.state.get('attempted_version'):
            return {'ok': False, 'error': 'No update version selected'}
        try:
            self._set_state('APPLYING')
            version = self.state['attempted_version']
            staging_dir = os.path.join(STAGING_DIR, f'v{version}')

            if not os.path.exists(staging_dir):
                self._set_state('FAILED')
                return {'ok': False, 'error': 'Staging dir not found'}

            # 备份
            backup_dir = os.path.join(BACKUP_DIR, f'v{self.state["current_version"]}')
            os.makedirs(backup_dir, exist_ok=True)

            backup_paths = [
                '/usr/local/bin/ttbox-core',
                '/usr/local/bin/ttbox-web',
                '/etc/ttbox/ttbox.conf',
            ]
            for src in backup_paths:
                if os.path.exists(src):
                    dst = os.path.join(backup_dir, os.path.relpath(src, '/'))
                    os.makedirs(os.path.dirname(dst), exist_ok=True)
                    shutil.copy2(src, dst)

            self.state['backup_path'] = backup_dir
            self.state['previous_version'] = self.state['current_version']
            self.state['rollback_available'] = True
            self._save_state()

            # 应用新文件
            files_dir = os.path.join(staging_dir, f'TTBOX-core-{version}-rk3588', 'files')
            if os.path.exists(files_dir):
                for root, dirs, files in os.walk(files_dir):
                    for f in files:
                        src = os.path.join(root, f)
                        dst = os.path.join('/', os.path.relpath(root, files_dir), f)
                        os.makedirs(os.path.dirname(dst), exist_ok=True)
                        shutil.copy2(src, dst)
                        log.info(f'Installed: {dst}')

            self.state['current_version'] = version
            self.state['last_update_time'] = time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())
            self._save_state()

            self._set_state('HEALTH_CHECK')

            # 健康检查
            health_ok = self._health_check()
            if health_ok:
                self._set_state('COMMITTED')
                return {'ok': True, 'data': {'version': version, 'status': 'committed'}}
            else:
                log.warning('Health check failed, rolling back')
                self.rollback()
                return {'ok': False, 'error': 'Health check failed, rolled back'}

        except Exception as e:
            log.error(f'Apply failed: {e}')
            self.state['last_error'] = str(e)
            self._save_state()
            self.rollback()
            return {'ok': False, 'error': str(e)}

    def _health_check(self):
        """使用设备实际 systemd 服务进行健康检查。"""
        for service in ('ttbox-core', 'ttbox-web'):
            result = subprocess.run(['systemctl', 'is-active', '--quiet', service], timeout=10)
            if result.returncode != 0:
                log.warning(f'Health check failed: {service} is not active')
                return False
        return True

    def rollback(self):
        if not self.state['rollback_available']:
            return {'ok': False, 'error': 'No rollback available'}

        self._set_state('ROLLING_BACK')
        backup_dir = self.state['backup_path']

        if not os.path.exists(backup_dir):
            self._set_state('FAILED')
            return {'ok': False, 'error': 'Backup not found'}

        try:
            for root, dirs, files in os.walk(backup_dir):
                for f in files:
                    src = os.path.join(root, f)
                    dst = os.path.join('/', os.path.relpath(root, backup_dir), f)
                    os.makedirs(os.path.dirname(dst), exist_ok=True)
                    shutil.copy2(src, dst)

            self.state['current_version'] = self.state['previous_version']
            self.state['previous_version'] = ''
            self.state['rollback_available'] = False
            self.state['backup_path'] = ''
            self._save_state()

            self._set_state('ROLLED_BACK')
            log.info('Rollback completed')
            return {'ok': True, 'data': {'version': self.state['current_version']}}
        except Exception as e:
            log.error(f'Rollback failed: {e}')
            self.state['last_error'] = str(e)
            self._save_state()
            self._set_state('FAILED')
            return {'ok': False, 'error': str(e)}


    def scan_otg(self):
        """扫描 USB 设备查找 TTBOX 更新包"""
        self._set_state('CHECKING')
        try:
            import glob
            usb_mounts = ['/mnt/usb', '/media/usb', '/run/media']
            releases = []
            for mount in usb_mounts:
                if not os.path.exists(mount):
                    continue
                for manifest_path in glob.glob(os.path.join(mount, '**/manifest.json'), recursive=True):
                    try:
                        with open(manifest_path) as f:
                            manifest = json.load(f)
                            if 'version' in manifest and 'packages' in manifest:
                                releases.append({
                                    'version': manifest['version'],
                                    'channel': manifest.get('channel', 'stable'),
                                    'release_date': manifest.get('release_date', ''),
                                    'release_notes': manifest.get('release_notes', ''),
                                    'size': sum(p.get('size', 0) for p in manifest['packages']),
                                    'source': 'usb',
                                    'manifest_path': manifest_path
                                })
                    except Exception as e:
                        log.warning(f'Failed to read {manifest_path}: {e}')
            self._set_state('IDLE')
            return {'ok': True, 'data': {'releases': releases, 'count': len(releases)}}
        except Exception as e:
            log.error(f'USB scan failed: {e}')
            self.state['last_error'] = str(e)
            self.state['error_count'] += 1
            self._save_state()
            self._set_state('IDLE')
            return {'ok': False, 'error': str(e)}

    def cancel_update(self):
        """取消当前更新操作"""
        if self.state['state'] in ['IDLE', 'COMMITTED', 'ROLLED_BACK', 'FAILED']:
            return {'ok': True, 'data': {'message': 'No update in progress'}}
        self._set_state('IDLE')
        self.state['attempted_version'] = ''
        self.state['attempted_channel'] = ''
        self.state['last_error'] = 'Update cancelled by user'
        self._save_state()
        return {'ok': True, 'data': {'message': 'Update cancelled'}}

    def get_log(self):
        """获取更新日志"""
        try:
            if os.path.exists(LOG_FILE):
                with open(LOG_FILE, 'r') as f:
                    lines = f.readlines()
                    return {'ok': True, 'data': {'log': lines[-100:], 'count': len(lines)}}
            return {'ok': True, 'data': {'log': [], 'count': 0}}
        except Exception as e:
            return {'ok': False, 'error': str(e)}
    def start_update(self, version=None):
        check_result = self.check_update()
        if not check_result.get('ok'):
            return check_result
        if not check_result.get('data', {}).get('update_available'):
            return {'ok': True, 'data': {'message': 'Already up to date'}}

        dl_result = self.download_update(version)
        if not dl_result.get('ok'):
            return dl_result

        stage_result = self.stage_update()
        if not stage_result.get('ok'):
            return stage_result

        apply_result = self.apply_update()
        return apply_result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='TTBOX Update Engine')
    parser.add_argument('--server', default=DEFAULT_SERVER)
    parser.add_argument('--action', choices=['check', 'download', 'stage', 'apply', 'start', 'rollback', 'status', 'scan-otg', 'cancel', 'log'],
                       default='status')
    parser.add_argument('--version')
    parser.add_argument('--channel', default='stable')
    args = parser.parse_args()

    engine = UpdateEngine(server_url=args.server)
    engine.channel = args.channel

    actions = {
        'status': lambda: print(json.dumps(engine.get_status(), indent=2, ensure_ascii=False)),
        'check': lambda: print(json.dumps(engine.check_update(), indent=2, ensure_ascii=False)),
        'download': lambda: print(json.dumps(engine.download_update(args.version), indent=2, ensure_ascii=False)),
        'stage': lambda: print(json.dumps(engine.stage_update(), indent=2, ensure_ascii=False)),
        'apply': lambda: print(json.dumps(engine.apply_update(), indent=2, ensure_ascii=False)),
        'start': lambda: print(json.dumps(engine.start_update(args.version), indent=2, ensure_ascii=False)),
        'rollback': lambda: print(json.dumps(engine.rollback(), indent=2, ensure_ascii=False)),
        'scan-otg': lambda: print(json.dumps(engine.scan_otg(), indent=2, ensure_ascii=False)),
        'cancel': lambda: print(json.dumps(engine.cancel_update(), indent=2, ensure_ascii=False)),
        'log': lambda: print(json.dumps(engine.get_log(), indent=2, ensure_ascii=False)),
    }
    actions.get(args.action, lambda: print('Unknown action'))()
