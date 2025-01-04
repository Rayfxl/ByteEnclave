import sys
import os
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QAction, QFileDialog, QLabel, 
    QVBoxLayout, QWidget, QFontDialog, QLineEdit, QPushButton, 
    QHBoxLayout, QComboBox, QFormLayout, QDateTimeEdit, QSpinBox, 
    QStyleFactory, QProgressBar, QCheckBox, QMessageBox, QTextEdit, 
    QListView, QAbstractItemView, QListWidget
)
from PyQt5.QtCore import Qt, QDateTime, QThread, pyqtSignal
import byte_enclave_python

class BackupThread(QThread):
    """后台备份线程"""
    progress = pyqtSignal(str, int)  # 进度信号，参数为(消息, 百分比)
    finished = pyqtSignal(bool, str)  # 完成信号，参数为(是否成功, 错误信息)
    
    def __init__(self, source, target, options):
        super().__init__()
        self.source = source
        self.target = target
        self.options = options
    
    def run(self):
        try:
            self.progress.emit('正在准备备份...', 0)
            backup_manager = byte_enclave_python.BackupManager()
            
            # 计算总文件大小
            total_size = 0
            if os.path.isfile(self.source):
                total_size = os.path.getsize(self.source)
            else:
                for root, dirs, files in os.walk(self.source):
                    for file in files:
                        total_size += os.path.getsize(os.path.join(root, file))
            
            processed_size = 0
            def progress_callback(current_file, file_size):
                nonlocal processed_size
                processed_size += file_size
                progress = min(95, int(processed_size * 100 / total_size))
                self.progress.emit(f'正在备份: {current_file}', progress)
            
            result = backup_manager.backup(self.source, self.target, self.options)
            if result:
                self.progress.emit('备份完成', 100)
                self.finished.emit(True, '')
            else:
                self.finished.emit(False, '备份失败: 操作返回False')
        except Exception as e:
            import traceback
            error_msg = f'备份过程中发生错误:\n{str(e)}\n\n详细错误:\n{traceback.format_exc()}'
            self.finished.emit(False, error_msg)

class RestoreThread(QThread):
    """后台还原线程"""
    progress = pyqtSignal(str, int)  # 进度信号，参数为(消息, 百分比)
    finished = pyqtSignal(bool, str)  # 完成信号，参数为(是否成功, 错误信息)
    
    def __init__(self, source, target, options):
        super().__init__()
        self.source = source
        self.target = target
        self.options = options
    
    def run(self):
        try:
            self.progress.emit('正在准备还原...', 0)
            backup_manager = byte_enclave_python.BackupManager()
            
            # 获取备份文件大小
            total_size = os.path.getsize(self.source)
            processed_size = 0
            def progress_callback(current_file, file_size):
                nonlocal processed_size
                processed_size += file_size
                progress = min(95, int(processed_size * 100 / total_size))
                self.progress.emit(f'正在还原: {current_file}', progress)
            
            result = backup_manager.restore(self.source, self.target, self.options)
            if result:
                self.progress.emit('还原完成', 100)
                self.finished.emit(True, '')
            else:
                self.finished.emit(False, '还原失败: 操作返回False')
        except Exception as e:
            import traceback
            error_msg = f'还原过程中发生错误:\n{str(e)}\n\n详细错误:\n{traceback.format_exc()}'
            self.finished.emit(False, error_msg)

class VerifyThread(QThread):
    """后台验证线程"""
    progress = pyqtSignal(str, int)  # 进度信号，参数为(消息, 百分比)
    finished = pyqtSignal(bool, str)  # 完成信号，参数为(是否成功, 错误信息)
    
    def __init__(self, source, options):
        super().__init__()
        self.source = source
        self.options = options
    
    def run(self):
        try:
            self.progress.emit('正在验证备份...', 0)
            backup_manager = byte_enclave_python.BackupManager()
            
            # 获取备份文件大小
            total_size = os.path.getsize(self.source)
            processed_size = 0
            def progress_callback(current_file, file_size):
                nonlocal processed_size
                processed_size += file_size
                progress = min(95, int(processed_size * 100 / total_size))
                self.progress.emit(f'正在验证: {current_file}', progress)
            
            result = backup_manager.verify_backup(self.source, self.options)
            if result:
                self.progress.emit('验证完成', 100)
                self.finished.emit(True, '')
            else:
                self.finished.emit(False, '备份验证失败')
        except Exception as e:
            import traceback
            error_msg = f'验证过程中发生错误:\n{str(e)}\n\n详细错误:\n{traceback.format_exc()}'
            self.finished.emit(False, error_msg)

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        # 保持对象引用
        self.backup_manager = byte_enclave_python.BackupManager()
        self.initUI()
        self.showWelcomePage()  # 显示欢迎页面

    def initUI(self):
        self.setWindowTitle('ByteEnclave 备份工具')
        self.resize(800, 600)
        self.center()

        # 设置全局样式
        QApplication.setStyle(QStyleFactory.create('Fusion'))
        self.setStyleSheet("""
            QLabel {
                font-size: 14px;
            }
            QLineEdit, QComboBox, QDateTimeEdit, QSpinBox {
                font-size: 14px;
                padding: 5px;
            }
            QPushButton {
                font-size: 14px;
                padding: 5px 10px;
                min-width: 80px;
            }
            QProgressBar {
                text-align: center;
                font-size: 12px;
            }
            QToolBar {
                spacing: 10px;
                padding: 5px;
            }
            QToolButton {
                font-size: 14px;
                padding: 5px 10px;
                min-width: 80px;
            }
        """)

        # 创建工具栏
        toolbar = self.addToolBar('主工具栏')
        toolbar.setMovable(False)  # 禁止移动工具栏
        
        # 创建备份和还原按钮
        backupAction = QAction('备份', self)
        restoreAction = QAction('还原', self)
        backupAction.setCheckable(True)
        restoreAction.setCheckable(True)
        
        # 添加按钮到工具栏
        toolbar.addAction(backupAction)
        toolbar.addAction(restoreAction)
        
        # 添加分隔符
        toolbar.addSeparator()
        
        # 添加设置按钮
        settingsAction = QAction('设置', self)
        toolbar.addAction(settingsAction)
        
        # 连接信号
        backupAction.triggered.connect(lambda: self.switchPage(backupAction, restoreAction, True))
        restoreAction.triggered.connect(lambda: self.switchPage(restoreAction, backupAction, False))
        settingsAction.triggered.connect(self.openSettings)

        # 默认显示备份界面
        backupAction.setChecked(True)
        self.showBackupPage()

    def showWelcomePage(self):
        # 创建新的中心部件
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        
        # 创建布局
        layout = QVBoxLayout()
        
        # 添加欢迎标签
        welcome_label = QLabel('欢迎使用 ByteEnclave 备份工具', self)
        welcome_label.setAlignment(Qt.AlignCenter)
        welcome_label.setStyleSheet("font-size: 24px; padding: 20px;")
        layout.addWidget(welcome_label)

        # 设置布局
        self.central_widget.setLayout(layout)

    def showBackupPage(self):
        # 创建新的中心部件
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.setWindowTitle('备份')

        # 重新创建备份管理器
        self.backup_manager = byte_enclave_python.BackupManager()

        # 创建布局
        layout = QVBoxLayout()
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(15)

        # 创建表单布局
        form_layout = QFormLayout()
        
        # 备份源路径
        source_layout = QHBoxLayout()
        self.source_path = QLineEdit()
        source_btn = QPushButton('浏览...')
        source_btn.clicked.connect(self.selectBackupSource)
        source_layout.addWidget(self.source_path)
        source_layout.addWidget(source_btn)
        form_layout.addRow('源路径:', source_layout)

        # 备份目标路径
        target_layout = QHBoxLayout()
        self.target_path = QLineEdit()
        target_btn = QPushButton('浏览...')
        target_btn.clicked.connect(self.selectBackupTarget)
        target_layout.addWidget(self.target_path)
        target_layout.addWidget(target_btn)
        form_layout.addRow('目标路径:', target_layout)

        # 备份选项
        self.include_hidden = QCheckBox('包含隐藏文件')
        form_layout.addRow('', self.include_hidden)

        # 排除模式
        self.exclude_pattern = QLineEdit()
        self.exclude_pattern.setPlaceholderText('例如: *.tmp;*.log')
        form_layout.addRow('排除模式:', self.exclude_pattern)

        # 加密选项
        encrypt_layout = QHBoxLayout()
        self.use_encryption = QCheckBox('启用加密')
        self.password = QLineEdit()
        self.password.setEchoMode(QLineEdit.Password)
        self.password.setEnabled(False)
        self.use_encryption.stateChanged.connect(lambda state: self.password.setEnabled(state == Qt.Checked))
        encrypt_layout.addWidget(self.use_encryption)
        encrypt_layout.addWidget(self.password)
        form_layout.addRow('加密:', encrypt_layout)

        # 添加表单到主布局
        layout.addLayout(form_layout)

        # 创建按钮
        button_layout = QHBoxLayout()
        backup_btn = QPushButton('开始备份')
        backup_btn.clicked.connect(self.startBackup)
        button_layout.addWidget(backup_btn)
        layout.addLayout(button_layout)

        # 进度显示
        self.progress_label = QLabel('')
        layout.addWidget(self.progress_label)
        self.progress_bar = QProgressBar()
        self.progress_bar.setVisible(False)
        layout.addWidget(self.progress_bar)

        # 设置布局
        self.central_widget.setLayout(layout)

    def showRestorePage(self):
        # 创建新的中心部件
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.setWindowTitle('还原')

        # 重新创建备份管理器
        self.backup_manager = byte_enclave_python.BackupManager()

        # 创建布局
        layout = QVBoxLayout()
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(15)

        # 备份文件列表
        self.backup_list = QListWidget()
        self.backup_list.itemSelectionChanged.connect(self.onBackupSelected)
        # 禁用双击编辑
        self.backup_list.setEditTriggers(QAbstractItemView.NoEditTriggers)
        layout.addWidget(QLabel('备份文件:'))
        layout.addWidget(self.backup_list)

        # 还原目标路径
        target_layout = QHBoxLayout()
        self.target_path = QLineEdit()
        target_btn = QPushButton('浏览...')
        target_btn.clicked.connect(self.selectRestoreTarget)
        target_layout.addWidget(self.target_path)
        target_layout.addWidget(target_btn)
        layout.addWidget(QLabel('还原目标:'))
        layout.addLayout(target_layout)

        # 加密选项
        encrypt_layout = QHBoxLayout()
        self.use_encryption = QCheckBox('启用加密')
        self.password = QLineEdit()
        self.password.setEchoMode(QLineEdit.Password)
        self.password.setEnabled(False)
        self.use_encryption.stateChanged.connect(lambda state: self.password.setEnabled(state == Qt.Checked))
        encrypt_layout.addWidget(self.use_encryption)
        encrypt_layout.addWidget(self.password)
        layout.addWidget(QLabel('加密:'))
        layout.addLayout(encrypt_layout)

        # 备份内容预览
        self.content_list = QTextEdit()
        self.content_list.setReadOnly(True)
        layout.addWidget(QLabel('备份内容:'))
        layout.addWidget(self.content_list)

        # 按钮
        button_layout = QHBoxLayout()
        restore_btn = QPushButton('开始还原')
        verify_btn = QPushButton('验证备份')
        restore_btn.clicked.connect(self.startRestore)
        verify_btn.clicked.connect(self.verifyBackup)
        button_layout.addWidget(restore_btn)
        button_layout.addWidget(verify_btn)
        layout.addLayout(button_layout)

        # 进度显示
        self.progress_label = QLabel('')
        layout.addWidget(self.progress_label)
        self.progress_bar = QProgressBar()
        self.progress_bar.setVisible(False)
        layout.addWidget(self.progress_bar)

        # 设置布局
        self.central_widget.setLayout(layout)

        # 刷新备份列表
        self.refreshBackupList()

    def refreshBackupList(self):
        """刷新备份文件列表"""
        self.backup_list.clear()
        # 搜索备份文件
        backup_dir = os.path.expanduser("~/data")  # 默认备份目录
        if os.path.exists(backup_dir):
            for file in os.listdir(backup_dir):
                # 只显示.backup文件，跳过所有临时文件
                if file.endswith('.backup') and not file.endswith('.tmp'):
                    full_path = os.path.join(backup_dir, file)
                    # 确保不是临时文件
                    if not any(full_path.endswith(suffix) for suffix in ['.pack.tmp', '.gz.tmp', '.enc.tmp', '.dec.tmp']):
                        self.backup_list.addItem(full_path)

    def onBackupSelected(self):
        """当选择备份文件时更新内容预览"""
        try:
            items = self.backup_list.selectedItems()
            if not items:
                return
            
            backup_path = items[0].text()
            if not os.path.exists(backup_path):
                self.content_list.clear()
                self.content_list.setPlainText(f'错误: 备份文件不存在: {backup_path}')
                return

            # 使用已创建的备份管理器
            try:
                # 先尝试不带密码读取
                options = byte_enclave_python.BackupOptions()
                contents = self.backup_manager.list_backup_contents(backup_path, options)
                if contents:
                    # 显示内容
                    self.content_list.clear()
                    self.content_list.setPlainText('备份文件内容:\n' + '-' * 40 + '\n')
                    for item in contents:
                        self.content_list.append(str(item))
                else:
                    # 如果内容为空，可能是加密文件
                    self.content_list.clear()
                    self.content_list.setPlainText('这是一个加密的备份文件\n请在还原时提供正确的密码')
            except Exception as e:
                # 读取失败，说明是加密文件
                self.content_list.clear()
                self.content_list.setPlainText('这是一个加密的备份文件\n请在还原时提供正确的密码')
                print(f'读取备份内容时出错: {str(e)}')
                
        except Exception as e:
            import traceback
            error_msg = f'预览备份内容时发生错误:\n{str(e)}\n\n详细错误:\n{traceback.format_exc()}'
            print(error_msg)
            self.content_list.clear()
            self.content_list.setPlainText(error_msg)

    def selectBackupSource(self):
        dialog = QFileDialog(self)
        dialog.setWindowTitle('选择源目录')
        dialog.setFileMode(QFileDialog.Directory)
        dialog.setOption(QFileDialog.DontUseNativeDialog, True)
        
        # 获取对话框中的视图和模型
        view = dialog.findChild(QListView, 'listView')
        if view:
            view.setSelectionMode(QAbstractItemView.SingleSelection)
        
        # 显示隐藏文件
        if dialog.findChild(QCheckBox, 'show_hidden'):
            dialog.findChild(QCheckBox, 'show_hidden').setChecked(True)
        
        if dialog.exec_():
            selected = dialog.selectedFiles()
            if selected:
                self.source_path.setText(selected[0])

    def selectBackupTarget(self):
        dialog = QFileDialog(self)
        dialog.setWindowTitle('选择目标目录')
        dialog.setFileMode(QFileDialog.Directory)
        dialog.setOption(QFileDialog.DontUseNativeDialog, True)
        
        # 获取对话框中的视图和模型
        view = dialog.findChild(QListView, 'listView')
        if view:
            view.setSelectionMode(QAbstractItemView.SingleSelection)
        
        # 显示隐藏文件
        if dialog.findChild(QCheckBox, 'show_hidden'):
            dialog.findChild(QCheckBox, 'show_hidden').setChecked(True)
        
        if dialog.exec_():
            selected = dialog.selectedFiles()
            if selected:
                self.target_path.setText(selected[0])

    def startBackup(self):
        source = self.source_path.text()
        target = self.target_path.text()
        
        if not source or not target:
            QMessageBox.warning(self, '错误', '请选择源路径和目标路径')
            return

        # 检查源路径是否存在
        if not os.path.exists(source):
            QMessageBox.warning(self, '错误', f'源路径不存在: {source}')
            return

        # 检查目标路径
        try:
            os.makedirs(target, exist_ok=True)
        except Exception as e:
            QMessageBox.warning(self, '错误', f'无法创建目标目录: {str(e)}')
            return

        # 创建备份选项
        options = byte_enclave_python.BackupOptions()
        options.include_hidden_files = self.include_hidden.isChecked()
        options.exclude_patterns = self.exclude_pattern.text().split(';') if self.exclude_pattern.text() else []
        if self.use_encryption.isChecked():
            if not self.password.text():
                QMessageBox.warning(self, '错误', '启用加密时必须设置密码')
                return
            options.password = self.password.text()

        # 显示进度条
        self.progress_bar.setVisible(True)
        self.progress_bar.setValue(0)
        
        # 打印调试信息
        print(f"开始备份:")
        print(f"源路径: {source}")
        print(f"目标路径: {target}")
        print(f"包含隐藏文件: {options.include_hidden_files}")
        print(f"排除模式: {options.exclude_patterns}")
        print(f"使用加密: {self.use_encryption.isChecked()}")
        
        # 创建并启动备份线程
        self.backup_thread = BackupThread(source, target, options)
        self.backup_thread.progress.connect(lambda msg, pct: self.updateProgress(msg, pct))
        self.backup_thread.finished.connect(self.onBackupFinished)
        self.backup_thread.start()
        
        # 禁用开始按钮，避免重复点击
        self.sender().setEnabled(False)

    def updateProgress(self, message, percent=None):
        """更新进度信息"""
        self.progress_label.setText(message)
        if percent is not None:
            self.progress_bar.setValue(percent)

    def onBackupFinished(self, success, error_msg):
        """备份完成的回调"""
        if success:
            self.progress_bar.setValue(100)
            self.progress_label.setText('备份完成')
            QMessageBox.information(self, '成功', '备份已完成')
        else:
            print(error_msg)  # 打印错误信息到控制台
            QMessageBox.critical(self, '错误', error_msg)
        self.progress_bar.setVisible(False)
        # 重新启用开始按钮
        for btn in self.findChildren(QPushButton):
            if btn.text() == '开始备份':
                btn.setEnabled(True)

    def startRestore(self):
        # 获取选中的备份文件
        items = self.backup_list.selectedItems()
        if not items:
            QMessageBox.warning(self, '错误', '请选择备份文件')
            return
        source = items[0].text()
        
        target = self.target_path.text()
        if not target:
            QMessageBox.warning(self, '错误', '请选择还原目录')
            return

        # 创建还原选项
        options = byte_enclave_python.BackupOptions()
        if self.use_encryption.isChecked():
            if not self.password.text():
                QMessageBox.warning(self, '错误', '启用加密时必须设置密码')
                return
            options.password = self.password.text()

        # 显示进度条
        self.progress_bar.setVisible(True)
        self.progress_bar.setValue(0)
        
        # 打印调试信息
        print(f"开始还原:")
        print(f"源文件: {source}")
        print(f"目标路径: {target}")
        print(f"使用加密: {self.use_encryption.isChecked()}")
        
        # 创建并启动还原线程
        self.restore_thread = RestoreThread(source, target, options)
        self.restore_thread.progress.connect(lambda msg, pct: self.updateProgress(msg, pct))
        self.restore_thread.finished.connect(self.onRestoreFinished)
        self.restore_thread.start()
        
        # 禁用开始按钮，避免重复点击
        self.sender().setEnabled(False)

    def onRestoreFinished(self, success, error_msg):
        """还原完成的回调"""
        if success:
            self.progress_bar.setValue(100)
            self.progress_label.setText('还原完成')
            QMessageBox.information(self, '成功', '还原已完成')
        else:
            print(error_msg)  # 打印错误信息到控制台
            QMessageBox.critical(self, '错误', error_msg)
        self.progress_bar.setVisible(False)
        # 重新启用开始按钮
        for btn in self.findChildren(QPushButton):
            if btn.text() == '开始还原':
                btn.setEnabled(True)

    def verifyBackup(self):
        # 获取选中的备份文件
        items = self.backup_list.selectedItems()
        if not items:
            QMessageBox.warning(self, '错误', '请选择备份文件')
            return
        source = items[0].text()

        # 创建选项
        options = byte_enclave_python.BackupOptions()
        if self.use_encryption.isChecked():
            if not self.password.text():
                QMessageBox.warning(self, '错误', '启用加密时必须设置密码')
                return
            options.password = self.password.text()

        # 显示进度条
        self.progress_bar.setVisible(True)
        self.progress_bar.setValue(0)
        
        # 创建并启动验证线程
        self.verify_thread = VerifyThread(source, options)
        self.verify_thread.progress.connect(lambda msg, pct: self.updateProgress(msg, pct))
        self.verify_thread.finished.connect(self.onVerifyFinished)
        self.verify_thread.start()
        
        # 禁用验证按钮，避免重复点击
        self.sender().setEnabled(False)

    def onVerifyFinished(self, success, error_msg):
        """验证完成的回调"""
        if success:
            self.progress_bar.setValue(100)
            self.progress_label.setText('验证完成')
            QMessageBox.information(self, '成功', '备份验证通过')
        else:
            print(error_msg)  # 打印错误信息到控制台
            QMessageBox.critical(self, '错误', error_msg)
        self.progress_bar.setVisible(False)
        # 重新启用验证按钮
        for btn in self.findChildren(QPushButton):
            if btn.text() == '验证备份':
                btn.setEnabled(True)

    def clearLayout(self, widget):
        layout = widget.layout()
        if layout is not None:
            while layout.count():
                item = layout.takeAt(0)
                widget = item.widget()
                if widget is not None:
                    widget.deleteLater()

    def selectRestoreTarget(self):
        """选择还原目标目录"""
        dialog = QFileDialog(self)
        dialog.setWindowTitle('选择还原目标目录')
        dialog.setFileMode(QFileDialog.Directory)
        dialog.setOption(QFileDialog.DontUseNativeDialog, True)
        
        # 获取对话框中的视图和模型
        view = dialog.findChild(QListView, 'listView')
        if view:
            view.setSelectionMode(QAbstractItemView.SingleSelection)
        
        # 显示隐藏文件
        if dialog.findChild(QCheckBox, 'show_hidden'):
            dialog.findChild(QCheckBox, 'show_hidden').setChecked(True)
        
        if dialog.exec_():
            selected = dialog.selectedFiles()
            if selected:
                self.target_path.setText(selected[0])

    def openSettings(self):
        # TODO: 实现设置对话框
        QMessageBox.information(self, '设置', '设置功能正在开发中...')

    def center(self):
        qr = self.frameGeometry()
        cp = QApplication.desktop().availableGeometry().center()
        qr.moveCenter(cp)
        self.move(qr.topLeft())

    def switchPage(self, active_action, other_action, show_backup):
        """切换界面"""
        active_action.setChecked(True)
        other_action.setChecked(False)
        if show_backup:
            self.showBackupPage()
        else:
            self.showRestorePage()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())