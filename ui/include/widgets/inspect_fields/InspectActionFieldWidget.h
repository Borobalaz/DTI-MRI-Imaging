#pragma once

#include <functional>
#include <utility>

#include <QHBoxLayout>
#include <QPushButton>
#include <QString>
#include <QVariant>

#include "widgets/inspect_fields/IInspectWidget.h"

class InspectActionFieldWidget : public QPushButton, public IInspectWidget
{
public:
	explicit InspectActionFieldWidget(QWidget *parent = nullptr);

	InspectActionFieldWidget(QString fieldId,
													 QString displayName,
													 QString groupName,
													 QWidget *parent = nullptr)
		;

	IInspectWidget *addToLayout(QHBoxLayout *layout) override
	;

	QString fieldId() const override;
	QString displayName() const override;
	QString groupName() const override;
	bool isReadOnly() const override;

	void SetValue(const QVariant &) override
	;

	QVariant GetValue() const override
	;

	std::function<void()> actionCallback;

private:
	QString fieldIdValue;
	QString displayNameValue;
	QString groupNameValue;
};
