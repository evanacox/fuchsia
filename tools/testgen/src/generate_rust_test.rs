// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use anyhow::Result;
use std::io::Write;

use crate::test_code::{CodeGenerator, TestCodeBuilder};
use std::collections::BTreeSet;

const MOCK_FUNC_TEMPLATE: &'static str = include_str!("templates/template_rust_mock_function");
const TEST_FUNC_TEMPLATE: &'static str = include_str!("templates/template_rust_test_function");

pub struct RustTestCodeGenerator<'a> {
    pub code: &'a RustTestCode,
}

impl CodeGenerator for RustTestCodeGenerator<'_> {
    fn write_file<W: Write>(&self, writer: &mut W) -> Result<()> {
        let create_realm_func_start = r#"pub async fn create_realm() -> Result<RealmInstance, Error> {
    let builder = RealmBuilder::new().await?;
"#;

        let mut create_realm_impl = self.code.realm_builder_snippets.join("\n");
        create_realm_impl.push_str("\n");
        let create_realm_func_end = r#"
    let instance = builder.build().await?;
    Ok(instance)
}

"#;
        // Add import statements
        let all_imports = self.code.imports.clone().into_iter().collect::<Vec<_>>();
        let mut imports = all_imports.join("\n");
        imports.push_str("\n\n");
        writer.write_all(&imports.as_bytes())?;

        // Add constants, these are components urls
        let mut constants = self.code.constants.join("\n");
        constants.push_str("\n\n");
        writer.write_all(&constants.as_bytes())?;

        // Generate create_realm() function
        writer.write_all(&create_realm_func_start.as_bytes())?;
        writer.write_all(&create_realm_impl.as_bytes())?;
        writer.write_all(&create_realm_func_end.as_bytes())?;

        // Add mock implementation functions, one per component
        if self.code.mock_functions.len() > 0 {
            let mut mock_funcs = self.code.mock_functions.join("\n\n");
            mock_funcs.push_str("\n\n");
            writer.write_all(&mock_funcs.as_bytes())?;
        }

        // Add testcases, one per protocol
        let mut test_cases = self.code.test_case.join("\n\n");
        test_cases.push_str("\n");
        writer.write_all(&test_cases.as_bytes())?;

        Ok(())
    }
}

pub struct RustTestCode {
    /// library import strings
    pub imports: BTreeSet<String>,
    /// constant strings
    constants: Vec<String>,
    /// RealmBuilder compatibility routing code
    pub realm_builder_snippets: Vec<String>,
    /// testcase functions
    test_case: Vec<String>,
    // skeleton functions for implementing mocks
    mock_functions: Vec<String>,
    /// var name used in generated RealmBuilder code that refers to the
    /// component-under-test
    component_under_test: String,
}

impl TestCodeBuilder for RustTestCode {
    fn new(component_name: &str) -> Self {
        RustTestCode {
            realm_builder_snippets: Vec::new(),
            constants: Vec::new(),
            imports: BTreeSet::new(),
            test_case: Vec::new(),
            mock_functions: Vec::new(),
            component_under_test: component_name.to_string(),
        }
    }
    fn add_import<'a>(&'a mut self, import_library: &str) -> &'a dyn TestCodeBuilder {
        self.imports.insert(format!(r#"use {};"#, import_library));
        self
    }

    fn add_component<'a>(
        &'a mut self,
        component_name: &str,
        url: &str,
        const_var: &str,
        mock: bool,
    ) -> &'a dyn TestCodeBuilder {
        if mock {
            let mock_function_name = format!("{}_impl", component_name);
            self.realm_builder_snippets.push(format!(
                r#"    let {child_component} = builder.add_local_child(
        "{child_component}",
        move |handles: LocalComponentHandles| Box::pin({mock_function}(handles)), 
        ChildOptions::new()
    )
    .await?;"#,
                child_component = component_name,
                mock_function = &mock_function_name
            ));
        } else {
            self.constants.push(format!(r#"const {}: &str = "{}";"#, const_var, url).to_string());
            self.realm_builder_snippets.push(format!(
                r#"    let {child_component} = builder.add_child(
        "{child_component}",
        {url},
        ChildOptions::new()
    )
    .await?;"#,
                child_component = component_name,
                url = const_var
            ));
        }
        self
    }

    fn add_mock_impl<'a>(
        &'a mut self,
        component_name: &str,
        _protocol: &str,
    ) -> &'a dyn TestCodeBuilder {
        // Note: this function name must match the one we added in 'add_component'.
        let mock_function_name = format!("{}_impl", component_name);
        self.mock_functions.push(MOCK_FUNC_TEMPLATE.replace("FUNCTION_NAME", &mock_function_name));
        self
    }

    fn add_protocol<'a>(
        &'a mut self,
        protocol: &str,
        source: &str,
        targets: Vec<String>,
    ) -> &'a dyn TestCodeBuilder {
        let source_code = match source {
            "root" => "Ref::parent()".to_string(),
            "self" => format!("&{}", self.component_under_test),
            _ => format!("&{}", source),
        };

        let mut targets_code: String = "".to_string();
        for i in 0..targets.len() {
            let t = &targets[i];
            if t == "root" {
                targets_code.push_str(format!("{:>16}.to(Ref::parent())\n", " ").as_str());
            } else if t == "self" {
                targets_code
                    .push_str(format!("{:>16}.to(&{})\n", " ", self.component_under_test).as_str());
            } else {
                targets_code.push_str(format!("{:>16}.to(&{})\n", " ", source).as_str());
            }
        }
        self.realm_builder_snippets.push(format!(
            r#"    builder
        .add_route(
            Route::new()
                .capability(Capability::protocol_by_name("{protocol}"))
                .from({from})
{to},
        )
        .await?;"#,
            protocol = protocol,
            from = source_code,
            to = targets_code.trim_end()
        ));
        self
    }

    fn add_directory<'a>(
        &'a mut self,
        dir_name: &str,
        dir_path: &str,
        targets: Vec<String>,
    ) -> &'a dyn TestCodeBuilder {
        let mut targets_code: String = "".to_string();
        for i in 0..targets.len() {
            let t = &targets[i];
            if t == "root" {
                targets_code.push_str(format!("{:>16}.to(Ref::parent())\n", " ").as_str());
            } else if t == "self" {
                targets_code
                    .push_str(format!("{:>16}.to(&{})\n", " ", self.component_under_test).as_str());
            } else {
                targets_code.push_str(format!("{:>16}.to(&{})\n", " ", t).as_str());
            }
        }
        self.realm_builder_snippets.push(format!(
            r#"    builder
        .add_route(
            Route::new()
                .capability(Capability::directory("{dir}").path("{path}").rights("fio::RW_STAR_DIR"))
                .from(Ref::parent())
{to},
        )
        .await?;"#,
            dir = dir_name,
            path = dir_path,
            to = targets_code.trim_end()
        ));
        self
    }

    fn add_storage<'a>(
        &'a mut self,
        storage_name: &str,
        storage_path: &str,
        targets: Vec<String>,
    ) -> &'a dyn TestCodeBuilder {
        let mut targets_code: String = "".to_string();
        for i in 0..targets.len() {
            let t = &targets[i];
            if t == "root" {
                targets_code.push_str(format!("{:>16}.to(Ref::parent())\n", " ").as_str());
            } else if t == "self" {
                targets_code
                    .push_str(format!("{:>16}.to(&{})\n", " ", self.component_under_test).as_str());
            } else {
                targets_code.push_str(format!("{:>16}.to(&{})\n", " ", t).as_str());
            }
        }
        self.realm_builder_snippets.push(format!(
            r#"    builder
        .add_route(
            Route::new()
                .capability(Capability::storage("{dir}").path("{path}"))
                .from(Ref::parent())
{to},
        )
        .await?;"#,
            dir = storage_name,
            path = storage_path,
            to = targets_code.trim_end()
        ));
        self
    }

    fn add_test_case<'a>(&'a mut self, protocol: &str) -> &'a dyn TestCodeBuilder {
        let protocol_marker = format!("{}Marker", &protocol);
        self.test_case.push(
            TEST_FUNC_TEMPLATE
                .replace("MARKER_VAR_NAME", &protocol_marker.to_ascii_lowercase())
                .replace("MARKER", &protocol_marker)
                .replace("PROTOCOL", &protocol),
        );
        self
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn deduplicate_imports() {
        let mut output: Vec<u8> = vec![];
        let mut code = RustTestCode::new("test-component");
        code.add_import("example::Value");
        code.add_import("example::Value");
        code.add_import("example::Value2");
        RustTestCodeGenerator { code: &code }.write_file(&mut output).expect("write output");

        let lines = std::str::from_utf8(&output)
            .expect("output must be UTF-8")
            .split("\n")
            .collect::<Vec<_>>();
        assert!(lines.len() > 3);
        assert_eq!(lines[0], "use example::Value2;");
        assert_eq!(lines[1], "use example::Value;");
        assert_ne!(lines[2], "use example::Value;");
    }
}
